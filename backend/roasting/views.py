from django.conf import settings
from django.shortcuts import get_object_or_404
from rest_framework import generics, status
from rest_framework.exceptions import ValidationError, AuthenticationFailed
from rest_framework.permissions import IsAdminUser, AllowAny
from rest_framework.response import Response
from rest_framework.views import APIView

from .models import RoastBatch, RoastReading
from .serializers import (
    RoastBatchAdminSerializer,
    RoastReadingAdminSerializer,
    TraceBatchSerializer,
)

class TraceBatchView(generics.RetrieveAPIView):
    permission_classes = [AllowAny]
    serializer_class = TraceBatchSerializer
    queryset = RoastBatch.objects.select_related("product").prefetch_related("readings")
    lookup_field = "code"
    lookup_url_kwarg = "batch_code"

class AdminBatchListCreateView(generics.ListCreateAPIView):
    permission_classes = [IsAdminUser]
    serializer_class = RoastBatchAdminSerializer

    def get_queryset(self):
        qs = RoastBatch.objects.select_related("product").order_by("-roasted_at", "-id")
        status_param = self.request.query_params.get("status")
        if status_param:
            qs = qs.filter(status=status_param)
        return qs

class AdminBatchDetailView(generics.RetrieveUpdateAPIView):
    permission_classes = [IsAdminUser]
    serializer_class = RoastBatchAdminSerializer
    queryset = RoastBatch.objects.all()

class AdminBatchReadingsView(APIView):
    permission_classes = [IsAdminUser]

    def get(self, request, pk):
        batch = get_object_or_404(RoastBatch, pk=pk)
        readings = batch.readings.all()
        alerts = readings.filter(is_alert=True)
        return Response({
            "batch_id": batch.id,
            "batch_code": batch.code,
            "target_max_temp_c": batch.target_max_temp_c,
            "status": batch.status,
            "alert_count": alerts.count(),
            "alerts": RoastReadingAdminSerializer(alerts, many=True).data,
            "readings": RoastReadingAdminSerializer(readings, many=True).data,
        })

class IoTReadingCreateView(APIView):
    permission_classes = [AllowAny]
    authentication_classes = []

    def post(self, request):
        device_key = request.headers.get("X-Device-Key")
        expected_key = getattr(settings, "IOT_DEVICE_KEY", "roaster-secret-key-2026")
        if not device_key or device_key != expected_key:
            raise AuthenticationFailed("Thiếu hoặc sai khóa thiết bị (X-Device-Key).")

        batch_code = request.data.get("batch_code")
        elapsed_s = request.data.get("elapsed_s")
        temperature_c = request.data.get("temperature_c")

        if not batch_code or elapsed_s is None or temperature_c is None:
            raise ValidationError({"detail": "Cần đủ batch_code, elapsed_s, temperature_c."})

        batch = get_object_or_404(RoastBatch, code=batch_code)
        if batch.status != "roasting":
            raise ValidationError({"detail": "Chỉ nhận điểm đo khi mẻ rang đang ở trạng thái 'roasting'."})

        try:
            elapsed_val = int(elapsed_s)
            temp_val = float(temperature_c)
        except (ValueError, TypeError):
            raise ValidationError({"detail": "elapsed_s phải là số nguyên, temperature_c phải là số thực."})

        is_alert = temp_val > batch.target_max_temp_c

        reading = RoastReading.objects.create(
            batch=batch,
            elapsed_s=elapsed_val,
            temperature_c=temp_val,
            is_alert=is_alert,
        )

        return Response(
            RoastReadingAdminSerializer(reading).data,
            status=status.HTTP_201_CREATED,
        )