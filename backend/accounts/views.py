from rest_framework import generics, permissions
from rest_framework.response import Response
from rest_framework.views import APIView

from .serializers import RegisterSerializer


class RegisterView(generics.CreateAPIView):
    """
    POST /api/auth/register/
    Đăng ký tài khoản khách hàng.
    """

    serializer_class = RegisterSerializer
    permission_classes = [permissions.AllowAny]

    def create(self, request, *args, **kwargs):
        serializer = self.get_serializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        user = serializer.save()

        return Response(
            {
                "id": user.id,
                "username": user.username,
            },
            status=201,
        )


class CurrentUserView(APIView):
    """
    GET /api/auth/me/
    Trả thông tin người dùng đang đăng nhập.
    """

    permission_classes = [permissions.IsAuthenticated]

    def get(self, request):
        user = request.user

        return Response(
            {
                "id": user.id,
                "username": user.username,
                "is_staff": user.is_staff,
            }
        )
