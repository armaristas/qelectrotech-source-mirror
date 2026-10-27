# Terminal Flow integrado

## Acceso

En QET Faramax: **Numeración → Numeración automática de bornes → Terminal Flow**.
La primera pestaña conserva las opciones originales de QET.

Terminal Flow trabaja sobre los objetos del proyecto abierto, incluidos los cambios sin guardar. La vista previa no modifica el proyecto. **Aplicar al proyecto** añade una única operación al historial: **Ctrl+Z** la deshace y **Ctrl+Mayús+Z**, o la acción Rehacer de QET, la repite.

## Numeración por bornero

1. Indica el identificador (`X`, `-X`, etc.) y pulsa **Analizar proyecto**.
2. Activa los borneros que quieras modificar.
3. Configura por bornero los folios, la prioridad X/Y y el número inicial.
4. Activa **Sustituir numeraciones existentes** si quieres renumerar bornas que ya tienen número. Por defecto se conservan.
5. Activa la agrupación multipunto si usas sufijos A/B/C. A inicia un grupo nuevo y los sufijos siguientes comparten su número hasta la siguiente A o una borna sin sufijo.
6. Pulsa **Vista previa**, revisa la tabla y el diagnóstico, y aplica la propuesta.

Los folios se seleccionan por su posición en el proyecto, contando desde 1: `all`, `2-5`, `1,3-5`, `todas` o `*`. No son el texto personalizado del cajetín. Los rangos descendentes, como `5-3`, se interpretan como `3-5`.

El orden siempre recorre primero los folios. X ordena después por coordenada horizontal y luego vertical; Y invierte esas dos coordenadas. Se usa la posición del símbolo en la escena.

Ejemplo multipunto con inicio 5: `X1:A`, `X1:B`, `X1:C`, `X1:A` se convierte en `X1:5A`, `X1:5B`, `X1:5C`, `X1:6A`. Una segunda ejecución con las mismas opciones conserva esos sufijos.

## Rangos

Añade una fila por regla en **Modificar rangos**:

| Destino | Segundo campo | Ejemplo, incremento 2 |
| --- | --- | --- |
| Bornas | Bornero, por ejemplo `X1` | `X1:10` → `X1:12` |
| Cables | No se utiliza | Texto de conductor `10` → `12` |
| Mangueras | Delimitador, por ejemplo `-` | Texto de conductor `10-3` → `12-3` |

Se trasladan las convenciones del Terminal_Flow original: cables y mangueras se reconocen por el **texto de numeración del conductor**. Este apartado no renombra las entidades del gestor nativo de cables ni cambia la asignación de sus hilos.

Cada regla se evalúa sobre los valores originales. Una regla que convierte 10 en 12 no provoca que otra regla vuelva a transformar ese 12 en la misma operación. Los rangos que coinciden sobre el mismo destino se rechazan para que la propuesta sea inequívoca.

## Validación

- Las bornas marcadas `auto_num_locked` y los folios de solo lectura se conservan.
- Se comprueban las colisiones también contra bornas de folios excluidos. Para bornas se tiene en cuenta instalación y localización, incluidas las del bornero nativo cuando no están informadas en el elemento.
- Varios segmentos del mismo potencial pueden compartir número. Se rechazan propuestas que asignen un número ya utilizado por otro potencial o dejen números incompatibles dentro del potencial modificado.
- Las fórmulas se eliminan únicamente en los destinos modificados. La tabla lo indica antes de aplicar.
- Si el proyecto o la configuración cambian, se necesita una nueva vista previa. También se valida el estado inmediatamente antes de aplicar.
- Cambiar la etiqueta de una borna utiliza el comando nativo de QET, que notifica al gestor de borneros. No se genera un segundo bornero.

## Compilación y ejecución en Windows

Con el kit Qt6/MinGW instalado en este equipo:

```powershell
.\faramax\build-windows.ps1 -RunTests
.\faramax\abrir-qet-faramax.ps1
```

Los parámetros de `build-windows.ps1` permiten cambiar las rutas de Qt, compilador, CMake y Ninja. El lanzador utiliza un perfil de desarrollo en `build-faramax/profile` y la colección oficial descargada en el repositorio.

La primera compilación prepara una biblioteca SQLite local y verificada mediante `prepare-sqlite.ps1`. CMake comprueba que disponga de las funciones de serialización que necesita QET. Se usan cabeceras precompiladas para agilizar el desarrollo. El parámetro `-KeepDebugSymbols` conserva los símbolos de depuración en los ejecutables; por defecto se omiten para reducir su tamaño.

Las pruebas específicas están en `tst_terminalflow` y `tst_terminalflowproject`. Las regresiones relacionadas se comprueban con `tst_terminallabeldisplay` y `tst_terminalcableauto`.
