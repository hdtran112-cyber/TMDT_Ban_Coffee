from django.urls import path
from .views import (
    TraceBatchView,
    AdminBatchListCreateView,
    AdminBatchDetailView,
    AdminBatchReadingsView,
    IoTReadingCreateView,
)

urlpatterns = [
    path("trace/<str:batch_code>/", TraceBatchView.as_view()),
    path("admin/batches/", AdminBatchListCreateView.as_view()),
    path("admin/batches/<int:pk>/", AdminBatchDetailView.as_view()),
    path("admin/batches/<int:pk>/readings/", AdminBatchReadingsView.as_view()),
    path("iot/readings/", IoTReadingCreateView.as_view()),
]