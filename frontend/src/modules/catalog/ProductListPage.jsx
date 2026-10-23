import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { getProducts } from "../../api/catalog";

export default function ProductListPage() {
  const [products, setProducts] = useState([]);
  const [error, setError] = useState("");

  useEffect(() => {
    getProducts()
      .then(setProducts)
      .catch(() => setError("Không tải được danh sách sản phẩm."));
  }, []);

  if (error) return <p>{error}</p>;
  return (
    <div>
      <h1>Cà phê rang</h1>
      <ul>
        {products.map((p) => (
          <li key={p.id}>
            <Link to={`/products/${p.id}`}>{p.name}</Link>
            {" - "}{p.price_vnd.toLocaleString("vi-VN")}đ ({p.roast_level})
          </li>
        ))}
      </ul>
    </div>
  );
}
