from django.db import models
from django.utils import timezone
from catalog.models import Product

class RoastBatch(models.Model):
    ROAST_CHOICES = [
        ("light", "Rang nhạt"),
        ("medium", "Rang vừa"),
        ("dark", "Rang đậm"),
    ]
    STATUS_CHOICES = [
        ("roasting", "Đang rang"),
        ("finished", "Đã xong"),
    ]

    code = models.CharField(max_length=30, unique=True, editable=False)
    product = models.ForeignKey(Product, on_delete=models.CASCADE, related_name="roast_batches")
    roast_level = models.CharField(max_length=10, choices=ROAST_CHOICES)
    roasted_at = models.DateTimeField(default=timezone.now)
    green_weight_g = models.PositiveIntegerField()
    roasted_weight_g = models.PositiveIntegerField(null=True, blank=True)
    target_max_temp_c = models.FloatField()
    status = models.CharField(max_length=15, choices=STATUS_CHOICES, default="roasting")
    notes = models.TextField(blank=True)

    def save(self, *args, **kwargs):
        if not self.code:
            dt = self.roasted_at or timezone.now()
            date_str = dt.strftime("%Y%m%d")
            prefix = f"B{date_str}-"
            last_batch = RoastBatch.objects.filter(code__startswith=prefix).order_by("-code").first()
            if last_batch:
                try:
                    last_num = int(last_batch.code.split("-")[-1])
                    next_num = last_num + 1
                except ValueError:
                    next_num = 1
            else:
                next_num = 1
            self.code = f"{prefix}{next_num:02d}"
        super().save(*args, **kwargs)

    @property
    def freshness_days(self):
        delta = timezone.now().date() - self.roasted_at.date()
        return max(0, delta.days)

    @property
    def freshness_label(self):
        days = self.freshness_days
        if days <= 7:
            return "Rất tươi"
        elif days <= 21:
            return "Tươi"
        return "Nên dùng sớm"

    def __str__(self):
        return f"{self.code} - {self.product.name} ({self.status})"

class RoastReading(models.Model):
    batch = models.ForeignKey(RoastBatch, on_delete=models.CASCADE, related_name="readings")
    elapsed_s = models.PositiveIntegerField()
    temperature_c = models.FloatField()
    is_alert = models.BooleanField(default=False)
    recorded_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["elapsed_s"]

    def __str__(self):
        return f"{self.batch.code} @ {self.elapsed_s}s: {self.temperature_c}°C"

def get_latest_finished_batch_for_product(product_id):
    """
    Hàm tiện ích cung cấp cho module Catalog (Hiển):
    Lấy mẻ rang gần nhất đã hoàn thành ('finished') của một sản phẩm cụ thể.
    """
    return (
        RoastBatch.objects.filter(product_id=product_id, status="finished")
        .order_by("-roasted_at")
        .first()
    )