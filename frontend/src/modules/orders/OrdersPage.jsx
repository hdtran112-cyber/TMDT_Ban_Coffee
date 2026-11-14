import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { getOrders } from "../../api/orders";
import { formatPrice } from "../catalog/labels";
import { apiErrorText } from "./errors";
import { formatDateTime } from "./format";
import "./orders.css";

export default function OrdersPage() {
  const [orders, setOrders] = useState(null);
  const [loading, setLoading] = useState(true);
  const [needLogin, setNeedLogin] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    if (!localStorage.getItem("token")) {
      setNeedLogin(true);
      setLoading(false);
      return;
    }
    getOrders()
      .then(setOrders)
      .catch((err) => {
        console.error("Lỗi tải đơn hàng:", err);
        if (err.status === 401) setNeedLogin(true);
        else setError(apiErrorText(err, "Không tải được danh sách đơn hàng."));
      })
      .finally(() => setLoading(false));
  }, []);

  if (loading) return <p className="msg-muted">Đang tải...</p>;

  if (needLogin) {
    return (
      <div>
        <h1 className="page-title">Đơn của tôi</h1>
        <p className="msg-muted">Bạn cần đăng nhập để xem đơn hàng.</p>
        <Link to="/login?next=/orders" className="btn">Đăng nhập</Link>
      </div>
    );
  }

  if (error) return <p className="msg-error">{error}</p>;

  return (
    <div>
      <h1 className="page-title">Đơn của tôi</h1>
      <p className="page-sub">{orders.length} đơn hàng</p>

      {orders.length === 0 ? (
        <div>
          <p className="msg-muted">Bạn chưa có đơn hàng nào.</p>
          <Link to="/" className="btn">Xem sản phẩm</Link>
        </div>
      ) : (
        <div className="order-list">
          {orders.map((order) => (
            <Link key={order.id} to={`/orders/${order.id}`} className="order-card">
              <div>
                <span className="order-id">Đơn #{order.id}</span>
                <div className="order-meta">
                  {formatDateTime(order.created_at)} · {order.item_count} sản phẩm
                </div>
                <div className="badges">
                  <span className={`badge badge-${order.status}`}>{order.status_label}</span>
                  <span className={`badge badge-${order.payment_status}`}>{order.payment_status_label}</span>
                </div>
              </div>
              <span className="price">{formatPrice(order.total_vnd)}</span>
            </Link>
          ))}
        </div>
      )}
    </div>
  );
}
