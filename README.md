# Themis x64 (Unofficial)

Phần mềm chấm thi tự động **Themis** phục vụ các kỳ thi Học sinh giỏi Tin học, Olympic Tin học phiên bản **port 64-bit (x86_64)** dành cho cả **Linux** và **Windows x64** (Bản không chính thức).

Dự án được xây dựng và phát triển lại dựa trên phần mềm Themis nguyên bản của hai thầy **Lê Minh Hoàng** và **Đỗ Đức Đông** (Đại học Sư phạm Hà Nội).

---

## 🌟 Điểm nổi bật & Cải tiến trong bản x64

- **Kiến trúc 64-bit toàn diện:** Phá bỏ giới hạn 32-bit (x86) của Themis gốc. Có thể chấm bài và hỗ trợ các bài toán yêu cầu bộ nhớ RAM > 2GB.
- **Hỗ trợ đa nền tảng (Cross-platform):**
  - **Linux:** Đóng gói dạng **AppImage** độc lập, chạy trực tiếp trên hầu hết các bản phân phối Linux (Ubuntu, Debian, Fedora, Arch...).
  - **Windows x64:** Bản Portable giải nén chạy ngay, không cần cài đặt.
- **Tích hợp trình biên dịch & thông dịch hiện đại:**
  - Hỗ trợ **GCC / G++ mới nhất** (MinGW-w64 MCF x64 UCRT).
  - Cho phép tùy biến và hỗ trợ chấm **Python 3.11+ x64** trực tiếp thông qua đường dẫn cấu hình linh hoạt.
  - Hỗ trợ macro đường dẫn động `%APPDIR%` giúp Themis có tính di động tuyệt đối (copy thư mục đi bất kỳ ổ đĩa nào vẫn nhận đúng compiler và python đi kèm).
- **Giao diện hiện đại với Qt6:** Tối ưu hóa UI/UX, hỗ trợ hiển thị icon sắc nét, bảng cấu hình thân thiện.

---

## 📥 Tải về (Releases)

Các bản dựng tự động mới nhất có sẵn tại mục **[Releases](../../releases)**:
- **Linux:** Tải file `ThemisLinux-1.0-x86_64.AppImage`, cấp quyền thực thi (`chmod +x`) và chạy.
- **Windows:** Tải file `Themis-Windows.zip`, giải nén và chạy file `ThemisLinux.exe`.

---

## 🛠️ Biên dịch từ mã nguồn

### Yêu cầu:
- **Qt 6.5+** (yêu cầu các module `Qt6Widgets`, `Qt6Gui`, `Qt6Core`, `Qt6Xml`).
- Trình biên dịch C++ hỗ trợ C++17 trở lên (`g++` hoặc `clang++`).

### Build trên Linux:
```bash
git clone https://github.com/ubuntu2310fake/Themis-x64-unofficial.git
cd Themis-x64-unofficial
qmake6 ThemisLinux.pro
make -j$(nproc)
./ThemisLinux
```

---

## 📜 Giấy phép (License)

Dự án được phát hành theo giấy phép **GNU Affero General Public License v3.0 (AGPLv3)**. Xem chi tiết tại tệp [LICENSE](LICENSE).

Tác giả phần mềm Themis gốc: Thầy **Lê Minh Hoàng** & Thầy **Đỗ Đức Đông**.
