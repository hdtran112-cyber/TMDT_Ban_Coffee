from django.db.models import Max, Count, Q
from rest_framework import serializers
from catalog.models import Product
from .models import RoastBatch, RoastReading

class RoastReadingPublicSerializer(serializers.ModelSerializer):
    class Meta:
        model = RoastReading
        fields = ["elapsed_s", "temperature_c"]

class RoastReadingAdminSerializer(serializers.ModelSerializer):
    class Meta:
        model = RoastReading
        fields = ["id", "elapsed_s", "temperature_c", "is_alert", "recorded_at"]

class RoastBatchAdminSerializer(serializers.ModelSerializer):
    product_id = serializers.PrimaryKeyRelatedField(
        queryset=Product.objects.all(), source="product"
    )
    freshness_days = serializers.IntegerField(read_only=True)
    freshness_label = serializers.CharField(read_only=True)

    class Meta:
        model = RoastBatch
        fields = [
            "id", "code", "product_id", "roast_level", "roasted_at",
            "green_weight_g", "roasted_weight_g", "target_max_temp_c",
            "status", "notes", "freshness_days", "freshness_label",
        ]
        read_only_fields = ["id", "code", "freshness_days", "freshness_label"]

class TraceBatchSerializer(serializers.ModelSerializer):
    product = serializers.SerializerMethodField()
    freshness_days = serializers.IntegerField(read_only=True)
    freshness_label = serializers.CharField(read_only=True)
    summary = serializers.SerializerMethodField()
    readings = RoastReadingPublicSerializer(many=True, read_only=True)

    class Meta:
        model = RoastBatch
        fields = [
            "code", "product", "roast_level", "roasted_at",
            "freshness_days", "freshness_label", "summary", "readings",
        ]

    def get_product(self, obj):
        return {
            "id": obj.product.id,
            "name": obj.product.name,
            "bean_type": obj.product.bean_type,
        }

    def get_summary(self, obj):
        stats = obj.readings.aggregate(
            duration_s=Max("elapsed_s"),
            max_temp_c=Max("temperature_c"),
            alert_count=Count("id", filter=Q(is_alert=True)),
        )
        return {
            "duration_s": stats["duration_s"] or 0,
            "max_temp_c": stats["max_temp_c"] or 0.0,
            "alert_count": stats["alert_count"] or 0,
        }