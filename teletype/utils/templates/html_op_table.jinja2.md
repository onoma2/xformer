
<!--
make sure that the description column is long!
Pandoc uses the size of the header column to calculate the absolute column widths
(as Latex requires explict column sizes)
-->
<div class="AlphaOpList">
| `OP` includes set notation and aliases | Description                                         |
|----------------------------------|-----------------------------------------------------------|
{% for op in ops %}
| **`{{ op.prototype }}`** {% if op.prototype_set is defined %}<br />**`{{ op.prototype_set }}`**{% endif %} {% for a in op.aliases %}{% if loop.first %}<br />{% endif %} **`{{a}}`** {% if not loop.last %}, {% endif %} {% endfor %} | {{ op.short | replace("\n", " ") }} |
{% endfor %}
</div>