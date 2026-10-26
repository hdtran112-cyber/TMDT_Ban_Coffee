from django.db import migrations
from catalog.text import normalize_vi


def fill(apps, schema_editor):
    Product = apps.get_model("catalog", "Product")
    for p in Product.objects.all():
        p.search_name = normalize_vi(p.name)
        p.save(update_fields=["search_name"])

class Migration(migrations.Migration):

    dependencies = [
        ('catalog', '0002_product_search_name'),
    ]

    operations = [migrations.RunPython(fill, migrations.RunPython.noop)]
