from django.conf import settings
from django.shortcuts import get_object_or_404
from rest_framework import generics, status
from rest_framework.exceptions import AuthenticationFailed, ValidationError
from rest_framework.permissions import AllowAny, IsAdminUser
from rest_framework.response import Response
from rest_framework.views import APIView

from .models import RoastBatch, RoastReading
from .serializers import (
    RoastBatchAdminSerializer,
    RoastReadingAdminSerializer,
    TraceBatchSerializer,
)


# ==============================================================================
# 1. API TRUY XUẤT NGUỒN GỐC CÔNG KHAI (Dành cho khách hàng / Quét QR)
# GET /api/trace/<batch_code>/
# ==============================================================================
class TraceBatchView(generics.RetrieveAPIView):
    permission_classes = [AllowAny]
    serializer_class = TraceBatchSerializer
    queryset = RoastBatch.objects.select_related("product").prefetch_related("readings")
    lookup_field = "code"
    lookup_url_kwarg = "batch_code"


# ==============================================================================
# 2. CÁC API QUẢN TRỊ MẺ RANG (Yêu cầu tài khoản Admin có Token)
# ==============================================================================

# GET /api/admin/batches/?status=roasting | finished
# POST /api/admin/batches/ (Tạo mẻ rang mới)
class AdminBatchListCreateView(generics.ListCreateAPIView):
    permission_classes = [IsAdminUser]
    serializer_class = RoastBatchAdminSerializer

    def get_queryset(self):
        qs = RoastBatch.objects.select_related("product").order_by("-roasted_at", "-id")
        status_param = self.request.query_params.get("status")
        if status_param:
            qs = qs.filter(status=status_param)
        return qs


# GET /api/admin/batches/<id>/
# PATCH /api/admin/batches/<id>/ (Đổi status sang finished, cập nhật khối lượng)
class AdminBatchDetailView(generics.RetrieveUpdateAPIView):
    permission_classes = [IsAdminUser]
    serializer_class = RoastBatchAdminSerializer
    queryset = RoastBatch.objects.all()
    http_method_names = ["get", "patch", "head", "options"]


# GET /api/admin/batches/<id>/readings/ (Xem danh sách điểm đo và cảnh báo)
class AdminBatchReadingsView(APIView):
    permission_classes = [IsAdminUser]

    def get(self, request, pk):
        batch = get_object_or_404(RoastBatch, pk=pk)
        readings = batch.readings.all().order_by("elapsed_s")
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


# ==============================================================================
# 3. API DÀNH CHO THIẾT BỊ IOT (ESP32 / Script mô phỏng)
# POST /api/iot/readings/
# Xác thực bằng header: X-Device-Key
# ==============================================================================
class IoTReadingCreateView(APIView):
    permission_classes = [AllowAny]
    authentication_classes = []

    def post(self, request):
        # 1. Kiểm tra khóa thiết bị từ Header
        device_key = request.headers.get("X-Device-Key")
        expected_key = getattr(settings, "DEVICE_KEY", "")

        # Nếu server chưa thiết lập biến DEVICE_KEY hoặc gửi sai khóa -> 401
        if not expected_key or not device_key or device_key != expected_key:
            raise AuthenticationFailed("Thiếu hoặc sai khóa thiết bị (X-Device-Key).")

        # 2. Kiểm tra dữ liệu gửi lên
        batch_code = request.data.get("batch_code")
        elapsed_s = request.data.get("elapsed_s")
        temperature_c = request.data.get("temperature_c")

        if not batch_code or elapsed_s is None or temperature_c is None:
            raise ValidationError({"detail": "Cần đủ batch_code, elapsed_s, temperature_c."})

        # 3. Kiểm tra mẻ rang tồn tại
        batch = get_object_or_404(RoastBatch, code=batch_code)

        # 4. Chỉ nhận điểm đo khi mẻ đang roasting
        if batch.status != "roasting":
            raise ValidationError({"detail": "Chỉ nhận điểm đo khi mẻ rang đang ở trạng thái 'roasting'."})

        # 5. Ép kiểu dữ liệu và kiểm tra hợp lệ
        try:
            elapsed_val = int(elapsed_s)
            temp_val = float(temperature_c)
        except (ValueError, TypeError):
            raise ValidationError({"detail": "elapsed_s phải là số nguyên, temperature_c phải là số thực."})

        # 6. Tự động kiểm tra cảnh báo quá nhiệt
        is_alert = temp_val > batch.target_max_temp_c

        # 7. Lưu điểm đo vào database
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