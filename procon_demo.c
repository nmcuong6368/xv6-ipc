#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MSG_SIZE 128

// Tiến trình con số 1: Thực hiện vai trò đóng gói và gửi dữ liệu
void producer_process(int mq_id) {
    char msg_text[MSG_SIZE];
    
    // 1. Gửi thông điệp thuộc phân loại dữ liệu thông thường (Type 1)
    printf("1. [Producer] Chuan bi gui tin nhan phan loai Type 1...\n");
    strcpy(msg_text, "Data_Payload: Giao tiep du lieu cam bien luong cao (Type 1)");
    if(msgsnd(mq_id, 1, msg_text) == 0) {
        printf("-> [Producer] Gui thanh cong tin nhan vao hang doi!\n");
    }

    // 2. Gửi thông điệp thuộc lệnh điều khiển khẩn cấp hệ thống (Type 2)
    printf("2. [Producer] Chuan bi gui tin nhan phan loai Type 2...\n");
    strcpy(msg_text, "System_Command: Yeu cau he thong thuc thi Restart (Type 2)");
    if(msgsnd(mq_id, 2, msg_text) == 0) {
        printf("-> [Producer] Gui thanh cong lenh dieu khien vao hang doi!\n");
    }
    
    exit(0);
}

// Tiến trình con số 2: Thực hiện vai trò lọc và tiêu thụ thông điệp
void consumer_process(int mq_id) {
    char buf[MSG_SIZE];

    // CHỨNG MINH TÍNH NĂNG LỌC TIN NHẮN NÂNG CAO: Chủ động yêu cầu đọc Type 2 trước tiên
    printf("3. [Consumer] Dang yeu cau loc rieng tin nhan khan cap Type 2 (Blocking...)\n");
    if(msgrcv(mq_id, 2, buf) == 0) {
        printf("-> [Consumer] Kien chung da nhan dung tin nhan can loc: \"%s\"\n", buf);
    }

    // Sau khi xử lý xong lệnh khẩn cấp, tiến trình quay lại đọc tin nhắn thông thường Type 1
    printf("4. [Consumer] Dang yeu cau nhan tiep tin nhan thong thuong Type 1...\n");
    if(msgrcv(mq_id, 1, buf) == 0) {
        printf("-> [Consumer] Kien chung da nhan dung tin nhan can loc: \"%s\"\n", buf);
    }
    
    exit(0);
}

// Tiến trình cha điều phối vòng đời hàng đợi của hệ thống
int main(int argc, char *argv[]) {
    int project_key = 25237; // Định danh khóa Key chung độc quyền của đồ án
    int mq_id;

    printf("========= KHOI DONG HE THONG KIEM CHUNG IPC DEMO =========\n");

    // 1. Khởi tạo và cấp phát hàng đợi thông qua bộ quản lý nhân
    mq_id = msgget(project_key);
    if(mq_id < 0) {
        printf("Loi he thong khoi tao Message Queue!\n");
        exit(1);
    }
    printf("He thong cap phat thanh cong hang doi. MQ_ID index = %d\n", mq_id);

    // 2. Tách luồng tạo tiến trình Consumer tiêu thụ trước
    if(fork() == 0) {
        consumer_process(mq_id);
    }

    // Sử dụng hàm sleep chuẩn của không gian người dùng xv6
    sleep(3); 

    // 3. Tách luồng tạo tiến trình Producer bắn phá dữ liệu
    if(fork() == 0) {
        producer_process(mq_id);
    }

    // Đợi cả hai thực thể con hoàn thành chu trình xử lý
    wait(0);
    wait(0);

    // 4. Gọi lệnh giải phóng tài nguyên tĩnh, thu hồi vùng đệm Kernel Space
    printf("5. [Main_Process] Thuc hien thu hoi dong vong doi tai nguyen...\n");
    if(msgctl(mq_id, 0) == 0) {
        printf("========= NGHIEM THU HOAN THANH - HE THONG KHONG CO LOI =========\n");
    }
    
    exit(0);
}