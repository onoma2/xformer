
<!--
make sure that the description column is long!
Pandoc uses the size of the header column to calculate the absolute column widths
(as Latex requires explict column sizes)
-->
<div class="OpList">
| `OP`           | `OP (set)`         | `Alias` | Description                          |
|----------------------|--------------------|---------|----------------------------------|
{% for op in ops %}
| **`{{ op.prototype }}`** | {% if not op.prototype_set is defined %} - {% endif %} {% if op.prototype_set is defined %}**`{{ op.prototype_set }}`**{% endif %} | {% if not op.aliases is defined %} - {% endif %} {% for a in op.aliases %} **`{{a}}`** {% if not loop.last %}, {% endif %} {% endfor %} | {% if not op.description is defined %} {% if not op.short is defined %} - {% endif %} {{ op.short }} {% endif %} {{ op.description | replace("\n", "<br/>") }}
{% endfor %}
</div>