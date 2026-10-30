import { Link } from "react-router-dom";

const money = (value) =>
  new Intl.NumberFormat("vi-VN", {
    style: "currency",
    currency: "VND",
    maximumFractionDigits: 0,
  }).format(Number(value) || 0);

export default function CartPage({ cart, onUpdateQuantity, onRemove, onContinue, onCheckout }) {
  const subtotal = cart.reduce((sum, item) => sum + (Number(item.price_vnd) || 0) * (Number(item.quantity) || 0), 0);
  const totalItems = cart.reduce((sum, item) => sum + (Number(item.quantity) || 0), 0);

  return (
    <main className="cart-page">
      <div className="cart-page-heading">
        <span className="eyebrow">COFFEE HOUSE · GIỎ HÀNG</span>
        <h1>Giỏ hàng của bạn</h1>
        <p>Kiểm tra các sản phẩm bạn đã chọn trước khi tiếp tục.</p>
      </div>

      {cart.length === 0 ? (
        <section className="cart-empty">
          <span className="cart-empty-icon">☕</span>
          <h2>Giỏ hàng đang trống</h2>
          <p>Hãy khám phá bộ sưu tập cà phê và chọn hương vị yêu thích của bạn.</p>
          <button className="primary-button" type="button" onClick={onContinue}>Khám phá sản phẩm <span>→</span></button>
        </section>
      ) : (
        <div className="cart-layout">
          <section className="cart-items">
            <div className="cart-items-header"><strong>Sản phẩm đã chọn</strong><span>{totalItems} sản phẩm</span></div>
            {cart.map((item) => (
              <article className="cart-item" key={item.id}>
                <img src={item.image_url || item.image} alt={item.name} onError={(event) => { event.currentTarget.onerror = null; event.currentTarget.src = "https://images.unsplash.com/photo-1442512595331-e89e73853f31?w=300&q=80"; }} />
                <div className="cart-item-info">
                  <p className="product-category">{item.roast_level || "COFFEE HOUSE"}</p>
                  <h3>{item.name}</h3>
                  <strong>{money(item.price_vnd)}</strong>
                  <button className="remove-item" type="button" onClick={() => onRemove(item.id)}>Xóa sản phẩm</button>
                </div>
                <div className="quantity-control" aria-label={`Số lượng ${item.name}`}>
                  <button type="button" aria-label="Giảm số lượng" onClick={() => onUpdateQuantity(item.id, item.quantity - 1)}>−</button>
                  <span>{item.quantity}</span>
                  <button type="button" aria-label="Tăng số lượng" onClick={() => onUpdateQuantity(item.id, item.quantity + 1)}>+</button>
                </div>
                <strong className="cart-line-total">{money((Number(item.price_vnd) || 0) * item.quantity)}</strong>
              </article>
            ))}
            <button className="continue-shopping" type="button" onClick={onContinue}>← Tiếp tục mua sắm</button>
          </section>

          <aside className="cart-summary">
            <h2>Tóm tắt đơn hàng</h2>
            <div className="summary-row"><span>Tạm tính ({totalItems} sản phẩm)</span><span>{money(subtotal)}</span></div>
            <div className="summary-row"><span>Phí vận chuyển</span><span className="shipping-note">Tính sau</span></div>
            <div className="summary-total"><span>Tổng tạm tính</span><strong>{money(subtotal)}</strong></div>
            <button className="checkout-button" type="button" onClick={onCheckout}>Tiến hành đặt hàng <span>→</span></button>
            <p className="summary-note">Phí vận chuyển và thông tin đặt hàng sẽ được xác nhận ở bước tiếp theo.</p>
          </aside>
        </div>
      )}
    </main>
  );
}
