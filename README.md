## 1. Thiết lập Môi trường Hệ thống
Để biên dịch nhân xv6 và chạy trình giả lập Qemu, hệ thống Linux cần được cài đặt các gói công cụ và trình biên dịch chéo (Cross-compiler) sau:

```bash
sudo apt-get update
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-unknown-elf
## 2. Mã nguồn Gốc
git clone [https://github.com/mit-pdos/xv6-riscv.git](https://github.com/mit-pdos/xv6-riscv.git)
cd xv6-riscv
3. Kiến trúc Đăng ký & Tích hợp Hệ thống (System Calls)
3.1. Định nghĩa Số hiệu cuộc gọi hệ thống
#define SYS_msgget 24
#define SYS_msgsnd 25
#define SYS_msgrcv 26
#define SYS_msgctl 27
#define SYS_sleep 28
3.2. Ánh xạ Hàm xử lý trong Hạt nhân (Kernel Space)
// Nguyên mẫu hàm xử lý mức Kernel
extern uint64 sys_msgget(void);
extern uint64 sys_msgsnd(void);
extern uint64 sys_msgrcv(void);
extern uint64 sys_msgctl(void);

// Đăng ký vào mảng con trỏ hàm syscalls[]
[SYS_msgget]    sys_msgget,
[SYS_msgsnd]    sys_msgsnd,
[SYS_msgrcv]    sys_msgrcv,
[SYS_msgctl]    sys_msgctl,
3.3. Cấu hình Khởi tạo Hệ thống Toàn cục
// msgqueue.c
void            msgqueueinit(void);
uint64          sys_msgget(void);
uint64          sys_msgsnd(void);
uint64          sys_msgrcv(void);
uint64          sys_msgctl(void);
4. Giao diện Phía Người Dùng (User Space API)
4.1. Khai báo thư viện người dùng (user/user.h)
int sleep(int);
// Các system calls phục vụ dự án đa hàng đợi tin nhắn có cấu trúc
int msgget(int);
int msgsnd(int, int, void*);
int msgrcv(int, int, void*);
int msgctl(int, int);
4.2. Tạo điểm kết nối Trap Assembly tự động (user/usys.pl)
entry("msgget");
entry("msgsnd");
entry("msgrcv");
entry("msgctl");
entry("sleep");
4.3. Đăng ký Đóng gói Biên dịch (Makefile)
# Thêm vào danh mục đối tượng hạt nhân OBJS
$K/msgqueue.o \

# Thêm vào danh mục chương trình ứng dụng UPROGS
$U/_procon_demo \
$U/_stress_demo\
5. Danh mục các Tệp mã nguồn Phát triển mới
Hệ thống được cấu thành và nghiệm thu qua 3 tệp mã nguồn cốt lõi sau:

kernel/msgqueue.c: Chứa toàn bộ cấu trúc dữ liệu struct message, struct msg_queue, mảng tĩnh quản lý mq_table, cơ chế đồng bộ đa lõi Spinlock, cơ chế chặn giải phóng CPU sleep/wakeup và thuật toán tịnh tiến dịch mảng vòng.

user/procon_demo.c: Chương trình kiểm thử đồng bộ IPC theo mô hình Producer - Consumer cơ bản, chứng minh khả năng lọc dữ liệu nâng cao theo thuộc tính type.

user/stress_demo.c: Chương trình kiểm thử áp lực cao (Stress Test) bằng cách tạo ra vòng lặp sinh song hành đồng thời 24 tiến trình con (12 tiến trình gửi, 12 tiến trình nhận) bắn phá dữ liệu đan xen, chứng minh độ ổn định công nghiệp của nhân hệ điều hành khi chạm ngưỡng tới hạn đầy/rỗng.
6. Hướng dẫn Vận hành và Thực nghiệm
# Sửa đổi hoặc xóa file Assembly cũ để ép sinh lại từ usys.pl
rm -f user/usys.S

# Dọn dẹp và đóng gói biên dịch lại toàn bộ dự án
make clean
make qemu
Chạy thử nghiệm cơ bản:


$ procon_demo
Chạy thử nghiệm áp lực cao (Stress Test 24 tiến trình):


$ stress_demo
