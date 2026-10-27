# QET Faramax

## Objetivo

Integrar Terminal_Flow, GNerador_Flechas, GNerador_TB, Marker_Pro y 4Mateo en QElectroTech. Las operaciones deben actuar sobre el proyecto abierto, incluidos los cambios todavía sin guardar, y permitir deshacer y rehacer sin cerrar el proyecto ni generar archivos QET intermedios.

## Estado del desarrollo (27 de octubre de 2026)

- Base inicial descargada de `qelectrotech/qelectrotech-source-mirror`: `bf0aaca7149f530bbb2a59928a5da6168ee9af5f`. Incorporadas las dos actualizaciones posteriores del fork; base actual: `6891413a77215412b12904e366e17d52ec26d772`.
- Proyecto C++17 y Qt6, versión declarada en CMake: 0.200.1.
- Fork de desarrollo indicado por el usuario: `armaristas/qelectrotech-source-mirror` (público), configurado como `origin`. El repositorio oficial permanece como `upstream`. El destino inicial `armaristas/QET_Faramax` queda sustituido por este fork.
- Copias de referencia de los cinco programas y del catálogo de Marker en `faramax/reference`. Los originales externos se conservan. Los scripts Python son referencias y no se ejecutan desde QET.
- Terminal Flow portado a C++ dentro de la ventana existente de numeración automática de bornes: diagnóstico, configuración por bornero, multipunto, rangos, vista previa y comandos nativos de deshacer. Compilación y pruebas de integración verificadas. Véase [uso y alcance](TERMINAL_FLOW.md).
- Entorno localizado: Qt 6.11.1, MinGW 13.1, CMake y Ninja bajo `C:/Qt`. Submódulos y dependencias de compilación preparados. Las 14 pruebas del motor de Terminal Flow han pasado.
- GNerador_Flechas portado a campos dinámicos de QET con resolución de cadenas, selección de destinos y opción automática por proyecto. Compilación y pruebas de integración verificadas. Véase [uso y alcance](FLECHAS.md).
- GNerador_TB integrado sobre los borneros y planes nativos: tabla, INT/EXT, actualización desde el proyecto, puentes/peines, niveles/dobles, reservas, presentación, configuración e historial Excel. Véase [uso y alcance](GNERADOR_TB.md).
- Interfaz de GNerador TB reorganizada siguiendo la referencia v5.6: panel lateral, selector con cantidades, excepciones, búsqueda, mensajes, orden de columnas, muestras de color y ventana maximizable. Conserva las operaciones nativas y añade pruebas de interacción de la interfaz.
- Marker_Pro y 4Mateo siguen pendientes de portar.
- No se ha publicado una versión ni se han enviado cambios al proyecto oficial.

## Verificación realizada

En Windows, con Qt 6.11.1 y MinGW 13.1: compilación completa correcta; `qelectrotech.exe --version` devuelve `0.200.1-dev` y código 0. SQLite 3.54.1 compilado localmente desde el archivo oficial, verificado contra su SHA3-256 publicado, con las API de serialización comprobadas durante la configuración.

CTest: **6/6 baterías correctas, 0 fallos**, en 37,21 segundos: `tst_terminalflow`, `tst_terminalflowproject`, `tst_arrowtexts`, `tst_gneradortb`, `tst_terminalcableauto` y `tst_terminallabeldisplay`. Incluyen lectura de cambios sin guardar, planificación sin modificar el proyecto, colisiones, bloqueo y solo lectura, fórmulas, deshacer/rehacer, persistencia, conservación de propiedades de conductores, flechas en cadena y ciclos. Las 18 comprobaciones de GNerador TB también cubren conexiones entre folios, conservación de ajustes manuales, puentes/peines, reservas, multinivel, dobles, persistencia y XLSX. Las ventanas de Terminal Flow y GNerador TB y el plano generado se han renderizado y revisado. El XLSX exportado se ha abierto además con openpyxl para comprobar su compatibilidad.

Esta verificación automatizada no equivale a validar todavía todo el flujo de trabajo sobre los proyectos reales del usuario. Marker_Pro y 4Mateo siguen pendientes. La validación de la ampliación GNerador TB se recoge en su documentación.

## Puntos de integración comprobados en el código

| Herramienta | Comportamiento de referencia | Base nativa que se debe reutilizar |
| --- | --- | --- |
| Terminal_Flow v2 | Diagnóstico de duplicados y huecos; numeración por folios, posición X/Y, inicio y multipunto; cambios de rangos de bornas, cables y mangueras | `QETProject`, información de elementos, comandos de edición y modelo de borneros |
| GNerador_Flechas V5 | Resolver etiquetas desde bornas y conexiones, seguir referencias entre flechas y ofrecer elección ante varios destinos | `ReportElement`, terminales, potenciales y enlaces; revisar creación y eliminación de conductores para actualización automática |
| GNerador_TB v5.6 | Clasificación INT/EXT, reservas, multinivel, puentes, colores, distribución y datos de conexión | `TerminalStrip`, `RealTerminal`, `PhysicalTerminal`, editor y dibujantes nativos |
| Marker_Pro v2.1 | Catálogo, importación de símbolos, cantidades detectadas o manuales, inserción y sustitución masiva | Importación a colección embebida del proyecto y comandos nativos de inserción; identificar lo generado mediante metadatos persistentes |
| 4Mateo v4.5.1 | Limpieza, formato de folios, numeraciones, aislamiento, alineación y visibilidad | Propiedades de conductores, textos y folios con comandos de deshacer; traducir las reglas del XML a propiedades del modelo |

El gestor actual ya dispone de agrupación, puentes, reservas, datos de cable y representación reactiva. `TerminalStripItem::connectStrip()` escucha `orderChanged`, `bridgeChanged`, `bridgeColorChanged` y `dataChanged`. La integración GNerador TB añade observación de conexiones, destinos, cables, referencias y folios, con notificaciones agrupadas y sin modificaciones automáticas de la pila de deshacer. La pertenencia a los borneros se cambia mediante las acciones nativas de asignación y reagrupación.

`TerminalStripAutoImport` ya asigna bornas por prefijo de etiqueta y contempla reagruparlas. Evitar un segundo modelo paralelo de borneros.

QET incluye `sources/scripting/qetscriptapi.*`: edición del proyecto vivo, elementos, propiedades de conductores, enlaces y borneros. Puede servir para prototipos verificables. No toda la API es reversible: sus comentarios identifican operaciones sin deshacer. Revisar cada operación antes de usarla. Las funciones automáticas ligadas a conexiones deberán integrarse con el ciclo de vida nativo.

## Reglas de implementación

1. Leer el modelo en memoria y respetar el proyecto activo y su modo de solo lectura.
2. Separar análisis, propuesta de cambios y aplicación. Una operación masiva debe presentar alcance, conflictos y cantidades antes de aplicarse.
3. Aplicar cada operación como una acción de deshacer coherente, conservando UUID, referencias y fórmulas salvo modificación explícita.
4. GNerador_Flechas debe asignar texto al conectar y recalcular al desconectar, cambiar etiquetas o modificar enlaces. No sobrescribir textos manuales sin una política visible; ante ambigüedad, pedir elección.
5. GNerador_TB debe ampliar los borneros nativos y mantener ajustes manuales. Actualizar desde señales, agrupar notificaciones y evitar ciclos de actualización.
6. Marker debe distinguir elementos generados de los insertados manualmente, para que repetir una operación no duplique ni borre componentes ajenos.
7. Conservar atribuciones y licencia del código original de QET. Revisar procedencia de recursos y licencias de las herramientas antes de una publicación.

## Secuencia y aceptación

1. Preparar compilación Qt6 y ejecutar las pruebas originales para disponer de una referencia verificable.
2. Integrar Terminal_Flow: diagnóstico y numeración; verificar colisiones, multipunto, filtros de folio y deshacer/rehacer.
3. Integrar Flechas: resolver recorridos, ciclos y destinos ambiguos; probar conexión y desconexión en vivo y etiquetas manuales.
4. Ampliar gestor de borneros con GNerador_TB: comparar filas y conexiones con el generador; probar reservas, puentes, multinivel y actualización sin regenerar archivos.
5. Integrar Marker: validar autodetección, catálogo, importación, sustitución e inserción reversible.
6. Integrar opciones de 4Mateo de forma independiente; contrastar sus reglas con proyectos de referencia.
7. Verificar conjuntamente edición sin guardar, dos proyectos abiertos, persistencia al guardar/reabrir, solo lectura y rendimiento con proyectos grandes.
8. Preparar una versión funcional y cambios pequeños para contribuir al proyecto original. Un fork vive en la cuenta del usuario; las contribuciones al original se proponen mediante pull requests.

## Compilación

Seguir `INSTALL.md` y `CONTRIBUTING.md` de esta revisión, incluidos submódulos, Qt6 GuiPrivate, LinguistTools, SQLite3 y Qt Test. En Windows el compilador debe coincidir con el kit Qt. El ejecutable de desarrollo está en `build-faramax/qelectrotech.exe`; el lanzador es `faramax/abrir-qet-faramax.ps1`.

## Siguiente hito

El siguiente módulo pendiente es Marker_Pro. GNerador TB necesita además contraste sobre proyectos reales del usuario para validar las convenciones INT/EXT, códigos de modelo y presentación habituales.
