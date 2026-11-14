import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { cancelOrder, getOrder, payOrder } from "../../api/orders";
import { formatPrice } from "../catalog/labels";
import { apiErrorText } from "./errors";
import { formatDateTime } from "./format";
import "./orders.css";

export default function OrderDetailPage() {
  const { id } = useParams();
  const [order, setOrder] = useState(null);
  const [loading, setLoading] = useState(true);
  const [needLogin, setNeedLogin] = useState(false);
  const [error, setError] = useState(""); // lỗi tải trang
  const [actionError, setActionError] = useState(""); // lỗi khi bấm Hủy / Thanh toán
  const [notice, setNotice] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    setNotice("");
    setActionError("");
    if (!localStorage.getItem("token")) {
      setNeedLogin(true);
      setLoading(false);
      return;
    }
    getOrder(id)
      .then(setOrder)
      .catch((err) => {
        console.error("Lỗi tải đơn:", err);
        if (err.status === 401) setNeedLogin(true);
        else if (err.status === 404) setError("Không tìm thấy đơn hàng.");
        else setError(apiErrorText(err, "Không tải được đơn hàng."));
      })
      .finally(() => setLoading(false));
  }, [id]);

  async function runAction(action, successText) {
    setBusy(true);
    setActionError("");
    setNotice("");
    try {
      setOrder(await action(order.id));
      setNotice(successText);
    } catch (err) {
      console.error("Lỗi thao tác đơn:", err);
      if (err.status === 401) setNeedLogin(true);
      else setActionError(apiErrorText(err, "Thao tác không thành công."));
    } finally {
      setBusy(false);
    }
  }

  function onCancel() {
    if (window.confirm("Bạn chắc chắn muốn hủy đơn này?")) {
      runAction(cancelOrder, "Đã hủy đơn hàng.");
    }
  }

  if (loading) return <p className="msg-muted">Đang tải...</p>;

  if (needLogin) {
    return (
      <div>
        <h1 className="page-title">Chi tiết đơn</h1>
        <p className="msg-muted">Bạn cần đăng nhập để xem đơn hàng.</p>
        <Link to="/login" className="btn">Đăng nhập</Link>
      </div>
    );
  }

  if (error) {
    return <p className="msg-error">{error} <Link to="/orders">Về đơn của tôi</Link></p>;
  }

  return (
    <div>
      <Link to="/orders" className="back">← Đơn của tôi</Link>

      <div className="order-head">
        <h1 className="page-title">Đơn #{order.id}</h1>
        <span className={`badge badge-${order.status}`}>{order.status_label}</span>
      </div>
      <p className="page-sub">Đặt lúc {formatDateTime(order.created_at)}</p>

      {notice && <p className="msg-ok">{notice}</p>}
      {actionError && <p className="msg-error">{actionError}</p>}

      <div className="order-grid">
        <div className="panel">
          <h2>Giao hàng và thanh toán</h2>
          <dl className="info-list">
            <dt>Người nhận</dt><dd>{order.shipping_name}</dd>
            <dt>Điện thoại</dt><dd>{order.phone}</dd>
            <dt>Địa chỉ</dt><dd>{order.address}</dd>
            <dt>Thanh toán</dt><dd>{order.payment_method_label}</dd>
            <dt>Tình trạng</dt>
            <dd>
              <span className={`badge badge-${order.payment.status}`}>{order.payment.status_label}</span>
            </dd>
            {order.payment.transaction_code && (
              <>
                <dt>Mã giao dịch</dt><dd>{order.payment.transaction_code}</dd>
              </>
            )}
            {order.payment.paid_at && (
              <>
                <dt>Thanh toán lúc</dt><dd>{formatDateTime(order.payment.paid_at)}</dd>
              </>
            )}
          </dl>
        </div>

        <div className="panel">
          <h2>Lịch sử trạng thái</h2>
          <ul className="timeline">
            {order.status_logs.map((log, index) => (
              <li key={index}>
                <strong>{log.status_label}</strong>
                <time>{formatDateTime(log.at)}</time>
              </li>
            ))}
          </ul>
        </div>
      </div>

      <div className="panel">
        <h2>Sản phẩm</h2>
        {order.items.map((item) => (
          <div key={item.id} className="item-row">
            <Link to={`/products/${item.product_id}`}>{item.product_name}</Link>
            <span className="msg-muted">{formatPrice(item.unit_price_vnd)} × {item.quantity}</span>
            <span className="line-total">{formatPrice(item.line_total_vnd)}</span>
          </div>
        ))}
        <div className="summary-total">
          <span>Tổng cộng</span>
          <span>{formatPrice(order.total_vnd)}</span>
        </div>

        {(order.can_pay || order.can_cancel) && (
          <div className="order-actions">
            {order.can_pay && (
              <button
                type="button"
                className="btn"
                disabled={busy}
                onClick={() => runAction(payOrder, "Thanh toán thành công (giả lập).")}
              >
                Thanh toán
              </button>
            )}
            {order.can_cancel && (
              <button type="button" className="btn btn-ghost" disabled={busy} onClick={onCancel}>
                Hủy đơn
              </button>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
