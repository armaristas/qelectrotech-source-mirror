# GNerador Flechas integrado

En **Numeración → GNerador Flechas — textos de destino** se muestran las flechas del proyecto abierto y los destinos conectados al otro extremo de sus referencias.

1. Selecciona las flechas que quieras preparar.
2. Revisa el destino propuesto. Si hay varias posibilidades, elige una en la lista.
3. Puedes activar **Añadir automáticamente este campo a nuevas flechas del proyecto**.
4. Pulsa **Aplicar**. La operación se puede deshacer desde QET.

La herramienta utiliza un campo de texto compuesto de QET con la variable `%{faramax_target}`. La configuración y el campo quedan guardados en el proyecto. Al conectar o desconectar conductores, cambiar enlaces o renombrar un dispositivo o borna, el texto se calcula desde las conexiones en memoria. Los textos manuales se conservan; la sustitución del primer texto manual requiere activar su casilla explícitamente. Si no hay un campo libre, se añade uno para el destino.

Un destino único se muestra directamente. Cuando hay varios, `?` señala una selección pendiente. La selección guarda la identidad del borne elegido y sigue sus cambios de etiqueta. Si deja de estar conectado y siguen existiendo varios destinos, se necesita otra selección. Una flecha sin enlace o sin destino muestra el campo vacío.

Se recorre la cadena de flechas con control de elementos visitados para evitar bucles. Una etiqueta de borna como `X1:5` se muestra completa; para un dispositivo con etiqueta `K1` y terminal `11`, se muestra `K1:11`.

Para el símbolo vacío `qfqfawe.elmt` superpuesto a una borna, se consulta la etiqueta de la borna real en el modelo. No hace falta generar atributos XML auxiliares en los conductores.

La opción automática afecta a los campos que se añaden a las flechas. Desactivarla conserva los campos ya preparados. Los textos existentes continúan actualizándose desde sus conexiones.

Las pruebas del módulo están en `tests/qttest/tst_arrowtexts.cpp`: actualización de etiquetas, desconexión y reconexión, conservación del texto manual, deshacer y rehacer, selección entre varios destinos, persistencia, cadenas y ciclos.
