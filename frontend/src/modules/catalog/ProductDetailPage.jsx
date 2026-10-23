import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { getProduct } from "../../api/catalog";

const ROAST_LABEL = { light: "Rang nhạt", medium: "Rang vừa", dark: "Rang đậm" };

export default function ProductDetailPage() {
  const { id } = useParams();
  const [product, setProduct] = useState(null);
  const [error, setError] = useState("");

  useEffect(() => {
    getProduct(id)
      .then(setProduct)
      .catch((err) =>
        setError(err.status === 404 ? "Không tìm thấy sản phẩm." : "Không tải được sản phẩm.")
      );
  }, [id]);

  if (error) return <p>{error} <Link to="/">Quay lại danh sách</Link></p>;
  if (!product) return <p>Đang tải...</p>;

  return (
    <div>
      <Link to="/">← Quay lại</Link>
      <h1>{product.name}</h1>
      {product.image_url && (
        <img src={product.image_url} alt={product.name} style={{ maxWidth: 320 }} />
      )}
      <p><b>{product.price_vnd.toLocaleString("vi-VN")}đ</b> / {product.weight_g}g</p>
      <p>{product.bean_type} · {ROAST_LABEL[product.roast_level]}</p>
      <p>{product.stock > 0 ? "Còn hàng" : "Hết hàng"}</p>
      <p>{product.description}</p>
      <button disabled title="Hiếu sẽ nối với giỏ hàng">Thêm vào giỏ</button>
    </div>
  );
}
