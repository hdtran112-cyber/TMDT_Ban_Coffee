import { useEffect, useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { getCart } from "../../api/cart";
import { createOrder } from "../../api/orders";
import { formatPrice } from "../catalog/labels";
import { apiErrorText, fieldErrors } from "./errors";
import "./orders.css";

const EMPTY_FORM = { shipping_name: "", phone: "", address: "", payment_method: "cod" };

export default function CheckoutPage() {
  const navigate = useNavigate();
  const [cart, setCart] = useState(null);
  const [loading, setLoading] = useState(true);
  const [needLogin, setNeedLogin] = useState(false);
  const [form, setForm] = useState(EMPTY_FORM);
  const [errors, setErrors] = useState({}); // lỗi theo từng ô nhập
  const [formError, setFormError] = useState(""); // lỗi chung (giỏ trống, hết hàng...)
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    if (!localStorage.getItem("token")) {
      setNeedLogin(true);
      setLoading(false);
      return;
    }
    getCart()
      .then(setCart)
      .catch((err) => {
        if (err.status === 401) setNeedLogin(true);
        else setFormError(apiErrorText(err, "Không tải được giỏ hàng."));
      })
      .finally(() => setLoading(false));
  }, []);

  const onChange = (e) => setForm({ ...form, [e.target.name]: e.target.value });

  async function onSubmit(e) {
    e.preventDefault();
    setSubmitting(true);
    setErrors({});
    setFormError("");
    try {
      const order = await createOrder(form);
      navigate(`/orders/${order.id}`);
    } catch (err) {
      console.error("Lỗi đặt hàng:", err);
      if (err.status === 401) {
        setNeedLogin(true);
      } else if (err.status === 400) {
        const fe = fieldErrors(err);
        setErrors(fe);
        setFormError(fe.cart || fe.detail || "");
      } else {
        setFormError(apiErrorText(err, "Không đặt được hàng, thử lại sau."));
      }
    } finally {
      setSubmitting(false);
    }
  }

  if (loading) return <p className="msg-muted">Đang tải...</p>;

  if (needLogin) {
    return (
      <div>
        <h1 className="page-title">Đặt hàng</h1>
        <p className="msg-muted">Bạn cần đăng nhập để đặt hàng.</p>
        <Link to="/login" className="btn">Đăng nhập</Link>
      </div>
    );
  }

  if (!cart) return <p className="msg-error">{formError || "Không tải được giỏ hàng."}</p>;

  if (cart.items.length === 0) {
    return (
      <div>
        <h1 className="page-title">Đặt hàng</h1>
        <p className="msg-muted">Giỏ hàng đang trống, chưa có gì để đặt.</p>
        <Link to="/" className="btn">Xem sản phẩm</Link>
      </div>
    );
  }

  return (
    <div>
      <Link to="/cart" className="back">← Quay lại giỏ hàng</Link>
      <h1 className="page-title">Đặt hàng</h1>
      <p className="page-sub">Điền thông tin giao hàng để hoàn tất đơn.</p>

      <div className="checkout">
        <form className="checkout-form" onSubmit={onSubmit}>
          <label className="field">
            <span>Họ tên người nhận</span>
            <input name="shipping_name" value={form.shipping_name} onChange={onChange} />
            {errors.shipping_name && <span className="msg-error">{errors.shipping_name}</span>}
          </label>

          <label className="field">
            <span>Số điện thoại</span>
            <input name="phone" inputMode="tel" placeholder="0900000000" value={form.phone} onChange={onChange} />
            {errors.phone && <span className="msg-error">{errors.phone}</span>}
          </label>

          <label className="field">
            <span>Địa chỉ giao hàng</span>
            <input name="address" value={form.address} onChange={onChange} />
            {errors.address && <span className="msg-error">{errors.address}</span>}
          </label>

          <label className="field">
            <span>Phương thức thanh toán</span>
            <select name="payment_method" value={form.payment_method} onChange={onChange}>
              <option value="cod">Thanh toán khi nhận hàng</option>
              <option value="qr_mock">Quét mã QR (giả lập)</option>
            </select>
            {errors.payment_method && <span className="msg-error">{errors.payment_method}</span>}
          </label>

          {formError && <p className="msg-error">{formError}</p>}

          <button type="submit" className="btn" disabled={submitting}>
            {submitting ? "Đang đặt hàng..." : "Xác nhận đặt hàng"}
          </button>
        </form>

        <div className="panel">
          <h2>Đơn hàng của bạn</h2>
          {cart.items.map((item) => (
            <div key={item.id} className="summary-line">
              <span>{item.product.name} × {item.quantity}</span>
              <span>{formatPrice(item.line_total_vnd)}</span>
            </div>
          ))}
          <div className="summary-total">
            <span>Tổng cộng</span>
            <span>{formatPrice(cart.total_vnd)}</span>
          </div>
        </div>
      </div>
    </div>
  );
}
