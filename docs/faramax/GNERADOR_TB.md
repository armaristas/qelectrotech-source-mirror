# GNerador TB dentro de QET

Integración C++/Qt de GNerador_TB v5.6 sobre `TerminalStrip`, `PhysicalTerminal`, `RealTerminal` y `TerminalStripItem`. No ejecuta Python ni modifica una copia del XML del proyecto. Los planos siguen siendo planes de borneros de QET, vinculados por UUID a los bornes originales.

## Uso

1. Abrir el gestor de borneros de QET y pulsar **GNerador TB** en su barra de herramientas.
2. Seleccionar el bornero activo en el panel lateral, que muestra sus cantidades. **Herramientas de tabla → Asignar / reagrupar** utiliza la importación de QET para asignar bornas libres o cambiar de bornero según prefijo, instalación y localización. No reordena los borneros existentes.
3. Revisar la tabla, los mensajes laterales y la vista previa. **Aplicar en QET** incorpora los cambios al proyecto abierto con Deshacer/Rehacer.
4. **Insertar plan en folio** activa la presentación GNerador TB e inserta el plan nativo en el folio activo. Si ya hay un plan de ese bornero en ese folio, lo muestra sin duplicarlo. Los planes del mismo bornero comparten sus ajustes.

La ventana es no modal: puede permanecer abierta mientras se modifica el esquema. Conserva los cambios pendientes y el texto que se está escribiendo; antes de aplicar comprueba si han cambiado los datos de la borna o la configuración en otra ventana.

## Interfaz de referencia

El editor se abre maximizado y puede restaurarse o redimensionarse. Mantiene el panel lateral del generador original: acciones principales, lista de borneros, excepciones EXT/INT, búsqueda y mensajes. La tabla usa el orden de columnas del original, con 21 columnas visibles por defecto. El tamaño del peine se muestra junto a su casilla y se ajusta en **Herramientas de tabla → Tamaño del peine**; su columna independiente puede mostrarse desde **Columnas**.

Los colores identifican bornero/número, puentes y reservas (verde), peines (naranja), cortes y destinos múltiples (azul), duplicados (morado) y datos que revisar (rojo). La columna Color comparte las reglas y el bicolor del plano. Las casillas se accionan haciendo clic en su celda. Las herramientas de copiar, pegar, ordenar, mover y gestionar reservas están en el menú superior **Herramientas de tabla** y en el menú contextual.

Las excepciones añadidas en el lateral recalculan la tabla como propuesta y conservan sus textos manuales pendientes. Se guardan con **Aplicar en QET** y pueden deshacerse. El selector lateral mantiene un bornero activo, con sus propios ajustes; la cabecera también informa de las cantidades totales del proyecto.

## Funciones de la referencia y adaptación nativa

| Función | Integración |
| --- | --- |
| Posición, número, tipo, modelo y bornero | Referencia calculada desde el folio y número editable; tipos de QET, referencia de fabricante y pertenencia al bornero nativo. Posición y pertenencia se consultan en la tabla; se cambian mediante el esquema y el gestor nativo. |
| INT / EXT | Clasificación por tipo del dispositivo, tierra y reglas `PREFIJO*\|EXT`, `PREFIJO*\|INT` o `PREFIJO=>INT`. Sigue enlaces entre flechas y termina ante ciclos. El destino real determina el tipo, también entre folios. |
| Dispositivos, cables y mangueras | Se leen de las conexiones actuales. Se prefiere el cable nativo asignado al conductor; sin cable, se utiliza el delimitador de manguera de v5.6. Se conservan todos los destinos detectados y se ofrecen en una lista editable. |
| Ajustes manuales | Se guardan solamente las sustituciones explícitas en los datos de la borna. Vaciar un texto manual recupera el cálculo automático. Cambiar otro campo no fija los datos calculados. |
| Puentes y peines | Modifican los puentes eléctricos del gestor, con validación de consecutividad y nivel. Se conserva el grupo anterior al deshacer una ampliación. El dibujo respeta el color del puente nativo. |
| Multinivel y dobles | **Multinivel** acepta `2`–`4`, `B`–`D`, nombres separados por comas o `=` para doble. Agrupa niveles nativos y crea reservas para los niveles adicionales; no inventa conexiones eléctricas. La columna Nivel, de 0 a 3, cambia el orden dentro de una borna ya agrupada. |
| Reservas | Detección de bornas sin conexión, marca manual, inserción y duplicado como reserva nativa. Las reservas automáticas se identifican para que cambiar su cantidad no elimine reservas ajenas. Se añaden al final del bornero, evitando etiquetas ocupadas. Cambiar la cantidad conserva la identidad y las anotaciones de las que permanecen. **Reservas manuales** permite dejar de gestionarlas automáticamente. |
| Orden, filas y edición masiva | Ordenación natural, mover, retirar del bornero, copiar/cortar/pegar y rellenar abajo/derecha. Retirar no borra el elemento del esquema. Deshacer restaura los mismos objetos, orden y puentes. |
| Cortes | Dividen la representación en tramos manteniendo un único bornero nativo. |
| Colores | Color nativo manual, bicolor, reglas por modelo y códigos de la configuración antigua. Admite los nombres HTML usados por v5.6. |
| SHL | Pantallas por lado, rangos de bornas, nombres de manguera y varios rangos separados por `&`. La pantalla interna utiliza la propiedad de pantalla de QET; los datos adicionales se guardan en la borna. |
| Presentación | Fuentes, tamaños, negritas, longitudes, orígenes, desplazamientos, marcos, línea de tierra y agrupación Normal/Compacta por lado. La ventana utiliza el tema azul de GNerador_TB, aplicado solo a este editor; el tema general de QET se conserva. |
| Configuración | Importación/exportación JSON y almacenamiento de los ajustes con el bornero dentro del proyecto. |
| Comprobaciones | Duplicados, huecos numéricos, destinos incompletos, ausencia de modelo, correspondencia PE/tierra, posición fuera de malla y reservas con conexiones. Son avisos de revisión; los cambios estructurales incompatibles se rechazan antes de modificar el proyecto. |
| Excel e historial | XLSX del bornero actual, filas seleccionadas, todos los borneros o cambios desde la última exportación de esta sesión. Primeras 22 columnas compatibles con v5.6; añade UUID y procedencia manual. Importa también historiales de v5.6. |
| Navegación | Las referencias del plano mantienen la navegación al elemento y los enlaces de exportación PDF de QET. Doble clic fuera de una referencia abre el editor integrado. |

## Actualización

Las señales de bornes, conductores, enlaces, cables y folios, junto con los cambios de Deshacer/Rehacer, se agrupan antes de recalcular. Solo se notifica al bornero si cambia su contenido; el dibujo se almacena en caché y los repintados no generan nuevas modificaciones. También se vigilan los folios cargados después de abrir el proyecto.

Una borna retirada del esquema deja de aparecer en el dibujo aunque su objeto siga vivo para Deshacer. Al reinsertarla, recupera su identidad. Renombrar un dispositivo o cable, cambiar una conexión o sus referencias recalcula los campos automáticos y mantiene los manuales.

La pertenencia, los niveles y los puentes los decide el gestor de QET. Cambiar un prefijo no traslada silenciosamente una borna a otro bornero: para ese cambio se utiliza **Asignar / reagrupar bornas**. Esto conserva las agrupaciones manuales y evita añadir operaciones automáticas a la pila mientras se está deshaciendo otra operación.

## Historiales y límites de compatibilidad

Recuperar un historial prepara cambios en la tabla para su revisión. Los archivos propios se vinculan por identidad nativa; los antiguos, por bornero y número únicos. Las filas ambiguas o sin correspondencia se cuentan y no se asignan por aproximación. Los textos de conexión del Excel solo se recuperan si se activa la opción correspondiente; las instantáneas calculadas no se convierten automáticamente en sustituciones manuales.

La importación no crea dispositivos ni conexiones que ya no existan en el proyecto. Los niveles nuevos se crean con **Multinivel** o el gestor de QET antes de recuperar sus anotaciones. El máximo de cuatro niveles coincide con el rango de edición empleado en esta integración. El Excel es un intercambio opcional: no es necesario para generar ni mantener el plano.

La validación automatizada está en `tst_gneradortb`; el archivo `fixtures/gneradortb_legacy.xlsx` es un historial de prueba sintético creado con openpyxl, no contiene datos del usuario. Se comprueban clasificación, actualización local y entre folios, edición manual y recuperación automática, eliminación/reinserción, puentes y peines, reservas, multinivel, dobles, persistencia, XLSX y apertura del editor.

## Verificación realizada

Windows, Qt 6.11.1 y MinGW 13.1: aplicación y traducciones compiladas; `--version` devuelve `0.200.1-dev` con salida 0. Las seis baterías de regresión pasan en 37,21 segundos. `tst_gneradortb` completa 18 comprobaciones sin fallos. Se han revisado las imágenes del editor y el plano y se ha abierto el XLSX exportado con openpyxl, verificando las 22 columnas compatibles más las dos columnas de identidad/procedencia, la hoja oculta y el tratamiento como texto de cadenas que empiezan por `=`.

La prueba de destinos largos detectó que el contorno automático de `QPicture` omitía textos girados en este entorno. El dibujante calcula sus límites con las métricas de fuente y avisa al elemento gráfico antes de cambiar su geometría, para evitar recortes en pantalla y PDF. Falta contrastar las convenciones y la presentación sobre los proyectos reales del usuario.

La revisión de interfaz incorpora pruebas de edición sobre el nuevo orden de columnas, coincidencia entre el color propuesto y el aplicado, casillas centradas, búsqueda y cambio de bornero sin modificar el proyecto, reglas EXT/INT con borradores pendientes y cierre del editor al destruir su proyecto. La captura de referencia se genera con datos sintéticos para la revisión visual.
