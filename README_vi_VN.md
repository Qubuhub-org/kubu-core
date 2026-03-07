<h1 align="center">
<img src="https://i.imgur.com/DDkfI9i.png" alt="Kubu" width="300"/>
<br/><br/>
Nhân Tố Kubu [KUBU, Ᵽ]
</h1>

Chọn ngôn ngữ: [EN](./README.md)| [CN](./README_zh_CN.md) | [PT](./README_pt_BR.md) | [FA](./README_fa_IR.md) | VI | [FR](./README_fr_FR.md) | [JA](./README_ja_JP.md) | [DE](./README_de_DE.md)

Kubu là một loại tiền điện tử tập trung vào cộng đồng được tạo ra bởi một trong những người sáng tạo Dogecoin ban đầu từ năm 2013. Nó được tạo ra với một mục đích, để tạo ra một cộng đồng mới và vui vẻ giống như cộng đồng Dogecoin ban đầu.

Không giống như các phiên bản trước đó, Kubu là một loại tiền tệ tầng 1. Điều này có nghĩa là không có hồ bơi thanh khoản để tiêu tốn, không có ví tiền bị đen danh sách và không có hợp đồng thông minh phức tạp. Kubu là một chuỗi khối đơn giản.

Giống như Dogecoin, phần mềm Nhân Tố Kubu cho phép bất kỳ ai cũng có thể vận hành một nút trong mạng lưới chuỗi khối Kubu và sử dụng phương pháp băm Scrypt cho Bằng Chứng Công Việc. Nó được điều chỉnh từ Bitcoin Core và các loại tiền điện tử khác.

Để biết thông tin về các mức phí mặc định được sử dụng trên mạng lưới Kubu, vui lòng tham khảo [đề xuất phí](doc/fee-recommendation.md).

**Trang web:** [kubucoin.org](https://kubucoin.org)

## Sự Khác Biệt với Dogecoin

Kubu là một nhánh của Dogecoin. Vì sự quen thuộc, chúng tôi sẽ cố gắng giữ cho Kubu giống với Dogecoin.

Thay đổi:

* Địa chỉ bắt đầu bằng `P` thay vì `D`
* Các tính năng BIPS sẽ bắt đầu từ khối 1000
* AuxPow bắt đầu từ khối 1500 (ID Chuỗi: 63)
* Giao diện đồ họa được thiết kế cho Kubu

## Sử Dụng 💻

Để bắt đầu hành trình của bạn với Nhân Tố Kubu, hãy xem [hướng dẫn cài đặt](INSTALL.md) và hướng dẫn [bắt đầu](doc/getting-started.md).

API JSON-RPC được cung cấp bởi Nhân Tố Kubu cho phép tự mô tả và có thể được duyệt bằng `kubu-cli help`, trong khi thông tin chi tiết cho mỗi lệnh có thể được xem bằng cách sử dụng `kubu-cli help <lệnh>`. Hoặc, xem [tài liệu Bitcoin Core](https://developer.bitcoin.org/reference/rpc/) - mà thực hiện một giao thức tương tự - để có phiên bản có thể duyệt.

### Cổng

Nhân Tố Kubu mặc định sử dụng cổng `33874` cho giao tiếp ngang hàng cần thiết để đồng bộ hóa chuỗi khối "mainnet" và cập nhật thông tin về giao dịch và khối mới. Ngoài ra, có thể mở một cổng JSONRPC, mặc định là cổng `33873` cho các nút mainnet. Rất khuyến nghị không tiết lộ các cổng RPC ra internet công cộng.

| Chức năng | mainnet | testnet | regtest |
| :------- | ------: | ------: | ------: |
| P2P      |   33874 |   44874 |   18444 |
| RPC      |   33873 |   44873 |   18332 |

## Phát Triển Liên Tục 💻

Nhân Tố Kubu là một phần mềm mã nguồn mở và do cộng đồng thúc đẩy. Quy trình phát triển là mở và được công khai; bất kỳ ai cũng có thể xem, thảo luận và làm việc trên phần mềm.

Các tài nguyên phát triển chính:

* [Dự Án GitHub](https://github.com/kubucoindev/kubu/projects) được sử dụng để theo dõi công việc đã được lên kế hoạch và đang tiến hành cho các bản phát hành sắp tới.
* [Thảo Luận GitHub](https://github.com/kubucoindev/kubu/discussions) được sử dụng để thảo luận về các tính năng, cả về những tính năng đã được lên kế hoạch và chưa được lên kế hoạch, liên quan đến cả phát triển phần mềm Nhân Tố Kubu, các giao thức cơ bản và tài sản KUBU.
* [Subreddit KubuDev](https://www.reddit.com/r/kubudev/)

### Chiến Lược Phiên Bản
Các số phiên bản tuân thủ cú pháp ```major.minor.patch```.

### Nhánh
Có 3 loại nhánh trong kho lưu trữ này:

- **master:** Ổn định, chứa phiên bản mới nhất của bản phát hành *major.minor* mới nhất.
- **maintenance:** Ổn định, chứa phiên bản mới nhất của các bản phát hành trước đó, vẫn đang được bảo trì. Định dạng: ```<phiên bản>-maint```
- **development:** Không ổn định, chứa mã nguồn mới cho các bản phát hành được lên kế hoạch. Định dạng: ```<phiên bản>-dev```

*Các nhánh Master và Maintenance chỉ có thể thay đổi bằng cách phát hành. Các bản phát hành đã được lên kế hoạch luôn có một nhánh phát triển và các yêu cầu kéo nên được gửi đối với những nhánh đó. Các nhánh Bảo dưỡng chỉ dành cho **sửa lỗi** thôi,***xin vui lòng gửi tính năng mới đối với nhánh phát triển với phiên bản cao nhất.***

## Đóng Góp 🤝

Nếu bạn phát hiện ra lỗi hoặc gặp vấn đề với phần mềm này, vui lòng báo cáo bằng cách sử dụng [hệ thống vấn đề](https://github.com/kubucoindev/kubu/issues/new?assignees=&labels=bug&template=bug_report.md&title=%5Bbug%5D+).

Vui lòng xem [hướng dẫn đóng góp](CONTRIBUTING.md) để biết cách bạn có thể tham gia vào việc phát triển Nhân Tố Kubu. Thường có [chủ đề cần trợ giúp](https://github.com/kubucoindev/kubu/labels/help%20wanted) nơi đóng góp của bạn sẽ có tác động lớn và nhận được sự đánh giá cao.

## Cộng Đồng 🐸

Bạn có thể tham gia vào các cộng đồng trên các phương tiện truyền thông xã hội khác nhau.
Để xem điều gì đang diễn ra, gặp gỡ mọi người và thảo luận, tìm ra hình ảnh mới nhất, tìm hiểu về Kubu, cung cấp hoặc yêu cầu sự trợ giúp, để chia sẻ dự án của bạn.

Dưới đây là một số nơi bạn có thể thăm:

* [r/Kubu](https://www.reddit.com/r/kubu/)
* [Discord](https://kubucoin.org/discord)
* [Twitter/X](https://twitter.com/KubuNetwork)

## Câu Hỏi Thường Gặp ❓

Bạn có câu hỏi liên quan đến Kubu? Có thể có một câu trả lời trong [FAQ](doc/FAQ.md) hoặc phần [hỏi đáp](https://github.com/kubucoindev/kubu/discussions/categories/q-a) của bảng thảo luận!

## Giấy Phép ⚖️
Nhân Tố Kubu được phát hành theo các điều khoản của giấy phép MIT. Xem
[COPYING](COPYING) để biết thêm thông tin hoặc xem
[opensource.org](https://opensource.org/licenses/MIT)
