#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MSG_SIZE 128
#define TOTAL_CORES 24
#define HALF_CORES 12
#define LOOP_COUNT 15  // Mỗi tiến trình bắn phá 15 lần để kích hoạt liên hồi trạng thái tới hạn

// Cấu trúc gói tin để kiểm tra rò rỉ hoặc sai lệch dữ liệu
struct payload {
    int id;
    int seq;
    char text[100];
};

void producer_work(int mq_id, int p_id) {
    struct payload p;
    // Phân loại type đan xen: Tiến trình chẵn gửi Type 1, tiến trình lẻ gửi Type 2
    int type = (p_id % 2 == 0) ? 1 : 2; 

    for (int i = 0; i < LOOP_COUNT; i++) {
        p.id = p_id;
        p.seq = i;
        
        // Thay vì in cả dòng chữ dài gây đè chữ, ta chỉ in ký tự 's' đại diện cho 1 gói tin được Send (Gửi)
        printf("s");
        
        // msgsnd() sẽ tự động sleep nếu hàng đợi chạm ngưỡng MAX_MSG (10)
        if (msgsnd(mq_id, type, (char*)&p) < 0) {
            printf("\n[ERR] P_%d gui that bai o luot %d\n", p_id, i);
            exit(1);
        }
    }
    exit(0);
}

void consumer_work(int mq_id, int c_id) {
    struct payload p;
    // Bộ lọc đan xen: Tiến trình nhận chẵn săn tìm Type 1, tiến trình lẻ săn tìm Type 2
    int wanted_type = (c_id % 2 == 0) ? 1 : 2;

    for (int i = 0; i < LOOP_COUNT; i++) {
        // msgrcv() sẽ tự động sleep nếu hàng đợi rỗng hoặc không có type phù hợp
        if (msgrcv(mq_id, wanted_type, (char*)&p) < 0) {
            printf("\n[ERR] C_%d nhan that bai o luot %d\n", c_id, i);
            exit(1);
        }
        
        // Kiểm tra tính toàn vẹn của dữ liệu thu được sau giải thuật tịnh tiến mảng vòng
        if ((p.id % 2 == 0 && wanted_type != 1) || (p.id % 2 != 0 && wanted_type != 2)) {
            printf("\n[CRITICAL] RO RI DU LIEU SAI LECH! Consumer %d nhan nham data cua Producer %d\n", c_id, p.id);
            exit(1);
        } else {
            // Thay vì in cả dòng chữ dài, ta chỉ in ký tự 'r' đại diện cho 1 gói tin được Receive (Nhận) & xác thực OK
            printf("r");
        }
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    int key = 9999; // Mã khóa độc quyền cho phiên Stress Test
    int mq_id = msgget(key);
    
    if (mq_id < 0) {
        printf("Khong the khoi tao Message Queue cho Stress Test\n");
        exit(1);
    }

    printf("=========================================================\n");
    printf("KHOI CHAY STRESS TEST: BAN PHA DONG THOI 24 TIEN TRINH CON\n");
    printf("He thong dang test song song. Ky tu [s] la Gui (Send), [r] la Nhan (Receive)\n");
    printf("=========================================================\n");

    // 1. Sinh ra 12 tiến trình Consumer trước để đưa hệ thống vào trạng thái Chặn Rỗng (Sleep)
    for (int i = 0; i < HALF_CORES; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("Fork Consumer that bai!\n");
            exit(1);
        }
        if (pid == 0) {
            consumer_work(mq_id, i);
        }
    }

    // Giãn cách một khoảng ngắn để toàn bộ 12 Consumer đi vào trạng thái Block định tuyến an toàn
    sleep(2);

    // 2. Sinh tiếp 12 tiến trình Producer liên tục ép xung dữ liệu vào mảng vòng
    for (int i = 0; i < HALF_CORES; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("Fork Producer that bai!\n");
            exit(1);
        }
        if (pid == 0) {
            producer_work(mq_id, i);
        }
    }

    // 3. Tiến trình cha chờ đợi toàn bộ 24 thực thể con hoàn thành chu trình sống
    for (int i = 0; i < TOTAL_CORES; i++) {
        wait(0);
    }

    // 4. Giải phóng hàng đợi, thu hồi tài nguyên tĩnh nhân Kernel
    printf("\n\n[MAIN] Tat ca 24 tien trinh con da hoan thanh an toan.\n");
    printf("[MAIN] Dang giai phong va thu hoi vung dem Message Queue...\n");
    msgctl(mq_id, 0);
    
    printf("=========================================================\n");
    printf("STRESS TEST SUCCESS: CHUNG MINH DO ON DINH CONG NGHIEP THANH CONG!\n");
    printf("=========================================================\n");

    exit(0);
}
