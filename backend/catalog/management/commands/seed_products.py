import os

from django.core.management.base import BaseCommand

from catalog.models import Product

# Ảnh nằm ở frontend/public/images/products/ và do Vite phục vụ.
# Đổi địa chỉ frontend bằng biến môi trường, ví dụ:
#   SEED_IMAGE_BASE=http://192.168.1.10:5173 python manage.py seed_products
IMAGE_BASE = os.environ.get("SEED_IMAGE_BASE", "http://localhost:5173").rstrip("/")

# (tên, mô tả, giá, tồn kho ban đầu, loại hạt, mức rang, khối lượng g, tên file ảnh)
# Danh sách theo tài liệu "09-du-lieu-san-pham-va-hinh-anh.md". Giá là dữ liệu minh họa.
SAMPLES = [
    ("Arabica Cầu Đất",
     "Hạt Arabica vùng cao Cầu Đất, rang nhạt, vị chua thanh, hương trái cây nhẹ.",
     180000, 40, "Arabica", "light", 500, "arabica-cau-dat.jpg"),
    ("Robusta Buôn Ma Thuột",
     "Robusta Đắk Lắk rang đậm, vị đắng đậm, hợp pha phin và cà phê sữa.",
     120000, 60, "Robusta", "dark", 500, "robusta-buon-ma-thuot.jpg"),
    ("Blend Sáng Tạo",
     "Phối Arabica và Robusta, rang vừa, cân bằng giữa vị chua và đắng.",
     150000, 25, "Blend", "medium", 500, "blend-sang-tao.jpg"),
    ("Culi Robusta Đắk Lắk",
     "Hạt culi tròn đều, rang đậm, vị mạnh và hậu vị kéo dài.",
     135000, 30, "Robusta", "dark", 500, "culi-robusta-dak-lak.jpg"),
    ("Moka Cầu Đất",
     "Giống Moka hạt nhỏ, rang nhạt, hương thơm phức hợp, sản lượng ít.",
     220000, 15, "Arabica", "light", 250, "moka-cau-dat.jpg"),
    ("Arabica Sơn La",
     "Arabica Tây Bắc, rang vừa, vị ngọt nhẹ, dễ uống.",
     165000, 35, "Arabica", "medium", 500, "arabica-son-la.jpg"),
    ("Robusta Honey Gia Lai",
     "Robusta sơ chế honey, rang vừa, vị đắng dịu và ngọt hơn Robusta thường.",
     145000, 20, "Robusta", "medium", 500, "robusta-honey-gia-lai.jpg"),
    ("Arabica Khe Sanh",
     "Arabica Quảng Trị, rang vừa, thân vị tròn, chua nhẹ.",
     170000, 25, "Arabica", "medium", 500, "arabica-khe-sanh.jpg"),
    ("Blend Espresso Đậm",
     "Phối trộn cho máy pha espresso, rang đậm, lớp crema dày.",
     140000, 40, "Blend", "dark", 500, "blend-espresso-dam.jpg"),
    ("Blend Phin Truyền Thống",
     "Phối trộn cho phin, rang đậm, đậm đà theo khẩu vị truyền thống.",
     125000, 50, "Blend", "dark", 500, "blend-phin-truyen-thong.jpg"),
]


class Command(BaseCommand):
    help = "Nạp 10 sản phẩm cà phê mẫu (chạy lại nhiều lần không bị trùng)"

    def handle(self, *args, **options):
        created_count = 0
        for name, desc, price, stock, bean, roast, weight, image in SAMPLES:
            _, created = Product.objects.update_or_create(
                name=name,
                # Cập nhật mỗi lần chạy: thông tin mô tả, giá, ảnh
                defaults={
                    "description": desc,
                    "price_vnd": price,
                    "bean_type": bean,
                    "roast_level": roast,
                    "weight_g": weight,
                    "image_url": f"{IMAGE_BASE}/images/products/{image}",
                    "is_active": True,
                },
                # Chỉ đặt khi tạo mới: tồn kho, để chạy lại không xóa
                # số lượng đã giảm do khách đặt hàng
                create_defaults={
                    "description": desc,
                    "price_vnd": price,
                    "stock": stock,
                    "bean_type": bean,
                    "roast_level": roast,
                    "weight_g": weight,
                    "image_url": f"{IMAGE_BASE}/images/products/{image}",
                },
            )
            created_count += created
        self.stdout.write(self.style.SUCCESS(
            f"Đã nạp {len(SAMPLES)} sản phẩm mẫu ({created_count} tạo mới, "
            f"{len(SAMPLES) - created_count} cập nhật). Ảnh lấy từ {IMAGE_BASE}"
        ))
