#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"

#define MAX_MQ 8
#define MAX_MSG 10
#define MSG_SIZE 128

// Cấu trúc dữ liệu thông điệp định dạng có cấu trúc
struct message {
    int type;                  // Chỉ số phân loại tin nhắn (Message Type)
    char data[MSG_SIZE];       // Vùng đệm lưu dữ liệu thô 128 bytes
};

// Cấu trúc quản lý trạng thái của một hàng đợi tuần hoàn
struct msg_queue {
    struct spinlock lock;       // Khóa Spinlock loại trừ tương hỗ trên môi trường đa lõi
    struct message msgs[MAX_MSG]; // Mảng vòng chứa danh sách tin nhắn bộ đệm
    int head;                  // Chỉ mục đầu mảng (Điểm rút tin nhắn)
    int tail;                  // Chỉ mục cuối mảng (Điểm thêm tin nhắn)
    int count;                 // Biến đếm số lượng tin nhắn hiện hữu trong queue
    int key;                   // Mã khóa định danh toàn cục để ánh xạ liên kết
    int active;                // Cờ Boolean (0/1) kiểm soát trạng thái cấp phát của hàng đợi
};

// Bảng tĩnh lưu trữ đa hàng đợi trong Kernel Space
struct msg_queue mq_table[MAX_MQ];

// Hàm thiết lập cấu hình ban đầu khi hệ thống boot nhân
void
msgqueueinit(void)
{
    for(int i = 0; i < MAX_MQ; i++) {
        initlock(&mq_table[i].lock, "msgqueue_lock");
        mq_table[i].active = 0;
        mq_table[i].key = 0;
        mq_table[i].head = 0;
        mq_table[i].tail = 0;
        mq_table[i].count = 0;
    }
}

// 1. System Call: msgget() - Khởi tạo hoặc liên kết hàng đợi tin nhắn
uint64
sys_msgget(void)
{
    int key;
    
    // SỬA ĐỔI: Gọi trực tiếp hàm void argint để bóc tách tham số thứ nhất
    argint(0, &key);

    // Vòng lặp 1: Kiểm tra xem mã khóa key đã được tiến trình khác đăng ký chưa
    for(int i = 0; i < MAX_MQ; i++) {
        acquire(&mq_table[i].lock);
        if(mq_table[i].active && mq_table[i].key == key) {
            release(&mq_table[i].lock);
            return i; // Khớp khóa thành công, trả về ID hàng đợi
        }
        release(&mq_table[i].lock);
    }

    // Vòng lặp 2: Nếu chưa có tiến trình nào đăng ký, cấp phát một khối tĩnh mới
    for(int i = 0; i < MAX_MQ; i++) {
        acquire(&mq_table[i].lock);
        if(!mq_table[i].active) {
            mq_table[i].active = 1;
            mq_table[i].key = key;
            mq_table[i].count = 0;
            mq_table[i].head = 0;
            mq_table[i].tail = 0;
            release(&mq_table[i].lock);
            return i; // Khởi tạo thành công
        }
        release(&mq_table[i].lock);
    }

    return -1; // Vượt quá giới hạn hàng đợi tối đa của nhân hệ thống
}

// 2. System Call: msgsnd() - Đẩy tin nhắn có phân loại vào hàng đợi
uint64
sys_msgsnd(void)
{
    int mq_id;
    int type;
    uint64 user_src; // Đổi sang kiểu uint64 để lưu con trỏ không gian ảo người dùng

    // SỬA ĐỔI: Gọi độc lập tuần tự các hàm trích xuất tham số kiểu void
    argint(0, &mq_id);
    argint(1, &type);
    argaddr(2, &user_src);

    if(mq_id < 0 || mq_id >= MAX_MQ) 
        return -1;

    struct msg_queue *mq = &mq_table[mq_id];
    acquire(&mq->lock);

    // Cơ chế chặn (Blocking Mode): Nếu hàng đợi bị đầy, tiến trình tự ngủ chặn để nhường CPU
    while(mq->count == MAX_MSG) {
        if(!mq->active) {
            release(&mq->lock);
            return -1; // Trả về lỗi nếu hàng đợi bị đóng/xóa đột ngột trong khi tiến trình đang ngủ
        }
        sleep(mq, &mq->lock); // Ngủ chặn trên kênh định danh địa chỉ hàng đợi
    }

    // Sao chép an toàn dữ liệu thô từ User-space vào vùng đệm Kernel-space
    if(copyin(myproc()->pagetable, mq->msgs[mq->tail].data, user_src, MSG_SIZE) < 0) {
        release(&mq->lock);
        return -1;
    }
    
    mq->msgs[mq->tail].type = type;
    mq->tail = (mq->tail + 1) % MAX_MSG; // Dịch chuyển tịnh tiến con trỏ Tail tuần hoàn theo mảng vòng
    mq->count++;

    wakeup(mq); // Đánh thức các tiến trình đang sleep chờ dữ liệu tại hàm msgrcv()
    release(&mq->lock);
    return 0;
}

// 3. System Call: msgrcv() - Nhận tin nhắn và lọc dữ liệu tịnh tiến mảng vòng
uint64
sys_msgrcv(void)
{
    int mq_id;
    int wanted_type;
    uint64 user_dst;

    // SỬA ĐỔI: Gọi độc lập tuần tự các hàm trích xuất tham số kiểu void
    argint(0, &mq_id);
    argint(1, &wanted_type);
    argaddr(2, &user_dst);

    if(mq_id < 0 || mq_id >= MAX_MQ) 
        return -1;

    struct msg_queue *mq = &mq_table[mq_id];
    acquire(&mq->lock);

    while(1) {
        if(!mq->active) {
            release(&mq->lock);
            return -1;
        }

        // Quét tuyến tính qua toàn bộ số lượng thông điệp thực tế xếp hàng trong bộ đệm nhân
        for(int i = 0; i < mq->count; i++) {
            int idx = (mq->head + i) % MAX_MSG;
            
            // Tìm thấy tin nhắn trùng khớp chỉ số bộ lọc phân loại cần tìm
            if(mq->msgs[idx].type == wanted_type) {
                // Đóng gói chuyển giao ngược thông tin từ Nhân ra không gian ảo người dùng nhận
                if(copyout(myproc()->pagetable, user_dst, mq->msgs[idx].data, MSG_SIZE) < 0) {
                    release(&mq->lock);
                    return -1;
                }

                // GIẢI THUẬT TỊNH TIẾN MẢNG VÒNG (Tuần 7): Dịch dời các tin nhắn đứng sau lên một bước
                for(int j = i; j < mq->count - 1; j++) {
                    int curr = (mq->head + j) % MAX_MSG;
                    int next = (mq->head + j + 1) % MAX_MSG;
                    mq->msgs[curr] = mq->msgs[next]; // Lấp chỗ trống tin nhắn vừa rút
                }
                
                // Hiệu chỉnh lùi vị trí Tail và trừ biến đếm
                mq->tail = (mq->tail - 1 + MAX_MSG) % MAX_MSG;
                mq->count--;

                wakeup(mq); // Đánh thức tiến trình gửi đang bị block do hàng đợi từng bị đầy
                release(&mq->lock);
                return 0; // Nhận thành công
            }
        }
        
        // Cơ chế chặn (Blocking Mode): Nếu hàng đợi rỗng hoặc chưa khớp loại tin nhắn, đi ngủ chặn giải phóng CPU
        sleep(mq, &mq->lock);
    }
}

// 4. System Call: msgctl() - Điều khiển, hủy bỏ hàng đợi thu hồi tài nguyên nhân
uint64
sys_msgctl(void)
{
    int mq_id;
    int cmd;

    // SỬA ĐỔI: Gọi độc lập trực tiếp hàm bóc tách đối số kiểu void
    argint(0, &mq_id);
    argint(1, &cmd);

    if(mq_id < 0 || mq_id >= MAX_MQ) 
        return -1;

    struct msg_queue *mq = &mq_table[mq_id];
    acquire(&mq->lock);

    if(!mq->active) {
        release(&mq->lock);
        return -1;
    }

    if(cmd == 0) { // Mã lệnh tiêu chuẩn đóng tài nguyên IPC_RMID = 0
        mq->active = 0;
        mq->key = 0;
        mq->count = 0;
        mq->head = 0;
        mq->tail = 0;
        
        wakeup(mq); // ĐÁNH THỨC toàn bộ tiến trình mồ côi đang kẹt ngủ chặn trên queue này để tự thoát lỗi
        release(&mq->lock);
        return 0; // Hủy thành công
    }

    release(&mq->lock);
    return -1; // Mã lệnh cmd không hợp lệ
}
