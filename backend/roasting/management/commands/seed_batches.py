from datetime import timedelta
from django.core.management.base import BaseCommand
from django.utils import timezone
from catalog.models import Product
from roasting.models import RoastBatch, RoastReading

class Command(BaseCommand):
    help = "Tạo dữ liệu mẫu cho module C (Roasting & IoT)"

    def handle(self, *args, **options):
        # 1. Đảm bảo có sản phẩm mẫu của module Catalog
        product, _ = Product.objects.get_or_create(
            id=1,
            defaults={
                "name": "Arabica Cầu Đất",
                "price_vnd": 150000,
                "stock": 50,
                "bean_type": "Arabica",
                "roast_level": "medium",
                "weight_g": 500,
            },
        )

        now = timezone.now()

        # 2. Tạo mẻ rang đã hoàn tất (status='finished')
        # Dùng get_or_create dựa theo notes hoặc thuộc tính cố định để chạy nhiều lần không lỗi trùng code
        batch_finished = RoastBatch.objects.filter(
            product=product, status="finished", notes="Mẻ mẫu hoàn tất (Demo)"
        ).first()

        if not batch_finished:
            batch_finished = RoastBatch.objects.create(
                product=product,
                roast_level="medium",
                roasted_at=now - timedelta(days=2),  # Rang cách đây 2 ngày (Rất tươi)
                green_weight_g=1200,
                roasted_weight_g=1010,
                target_max_temp_c=205.0,
                status="finished",
                notes="Mẻ mẫu hoàn tất (Demo)",
            )
            self.stdout.write(f"Đã tạo mẻ hoàn thành: {batch_finished.code}")

            # Tạo 15 điểm đo mô phỏng đường cong rang hạt cà phê chuẩn S-curve
            profile_curve = [
                (0, 185.0),
                (45, 115.0),
                (90, 95.0),    # Turning point (điểm hồi nhiệt)
                (150, 112.5),
                (210, 130.0),
                (270, 146.5),  # Giai đoạn sấy khô (Drying)
                (330, 161.0),
                (390, 172.5),  # Phản ứng Maillard
                (450, 183.0),
                (510, 191.5),
                (570, 198.0),  # First crack (nổ lần 1)
                (630, 202.5),
                (690, 204.8),
                (750, 205.5),  # Vượt ngưỡng nhẹ
                (780, 206.0),  # Kết thúc mẻ rang (Drop)
            ]

            readings = [
                RoastReading(
                    batch=batch_finished,
                    elapsed_s=sec,
                    temperature_c=temp,
                    is_alert=(temp > batch_finished.target_max_temp_c),
                )
                for sec, temp in profile_curve
            ]
            RoastReading.objects.bulk_create(readings)
            self.stdout.write(f"Đã nạp {len(readings)} điểm đo nhiệt độ cho mẻ {batch_finished.code}")
        else:
            self.stdout.write(f"Mẻ hoàn tất {batch_finished.code} đã tồn tại từ trước.")

        # 3. Tạo mẻ đang rang (status='roasting') chưa có điểm đo
        batch_roasting = RoastBatch.objects.filter(
            product=product, status="roasting", notes="Mẻ đang rang để test cảm biến IoT"
        ).first()

        if not batch_roasting:
            batch_roasting = RoastBatch.objects.create(
                product=product,
                roast_level="light",
                roasted_at=now,
                green_weight_g=1500,
                target_max_temp_c=200.0,
                status="roasting",
                notes="Mẻ đang rang để test cảm biến IoT",
            )
            self.stdout.write(f"Đã tạo mẻ đang rang: {batch_roasting.code}")
        else:
            self.stdout.write(f"Mẻ đang rang {batch_roasting.code} đã tồn tại từ trước.")

        self.stdout.write(self.style.SUCCESS("Hoàn tất seed dữ liệu mẻ rang!"))