# UI kit de los menús (docs/ui-kit.md)

Aprobado por el dueño (punto 25 de la revisión del showcase). Pedido del dueño («siempre cometéis los mismos fallos»: paginadores con otra letra, flechas desalineadas, márgenes de etiquetas, títulos de
tamaños distintos, cabeceras con borde que el original no tiene). Un catálogo medido, dónde se desvía cada pantalla nuestra y la API del módulo `game/frontend/ui_kit.c`, que se hace por pasos (una rama por
elemento). **Estado:** E4 (paginador) hecho: `ui_pager` / `ui_page_count`, `ui_kit_metrics.h`, `tests/test_ui_kit.c` y
`tools/ui_kit_check.sh` (mide el paginador en el binario y la imagen real; falla a más de 1 px). El resto está por hacer.

## 0. Método (todo se puede repetir)
- **Capturas reales** de 640x480 con `tools/menu_walk.sh` (origin/main `3c9d44bb`, Release, sin ventana ni sonido). Tres juegos de capturas:
  `cd` (`PROFILE=stock`: Game Mode, Control Central, Options, Car Showcase, la pantalla de pista, Best Times), `stock`/`resto` (las pantallas
  nuestras: Customize Game, Graphics Detail, Controls Setup, CONTENT, la página de una pista añadida, Control Central con una pista añadida,
  Best Times de Restomod). Las capturas de partida (`uikit-src-*`, `cd-*` del CD y `ours-*` nuestras) y los recortes están en la carpeta de notas del dueño, fuera del repo.
- **Medidas**: cajas de tinta de un color del juego en una zona (Pillow), con la tolerancia de abajo. El paginador se mide en el repo con
  `tools/ui_kit_pager_check.py`; las demás tablas, por ahora, a mano sobre las capturas. Una caja es `x0..x1` columnas y `y0..y1` filas; alto = y1 − y0 + 1; la tinta sólida con una
  tolerancia de 12-14 por canal (el borde suavizado y la sombra quedan fuera: ±1 px).
- **Código**: cada elemento lleva el `file:line` de lo que ya lo dibuja (origin/main `3c9d44bb`).
- Lo que no he medido en una captura dice **«sin medir»**; lo que sale solo de leer código, **«por código»**.

## 1. Lo común a todas las pantallas
| cosa | valor | de dónde |
|---|---|---|
| marco | 640x480; las pantallas 4:3 van centradas en pantallas anchas (+107 px en 16:9) | `tools/menu_walk.sh` (cabecera) |
| letra normal | `graver18`: mayúsculas de 15 px, línea de 24 | `game/sys/text.h:3-5,60` |
| letra pequeña | `MAIN18` del CD: mayúsculas de 13 px, línea de 20 (`text_small_*`) | `game/sys/text.h:56-62` |
| tamaños de la letra (num/den de `graver18`) | **3/4** = el tamaño de los botones y filas del CD (mayúscula de 11 px, medido en DONE y en «Road/track»); **3/5** = etiquetas de baldosa (9 px, medido); 2/3 cabeceras; 7/8 filas de Setup; **33/20** y **2/1** títulos que caen a la letra | `central_glue_plus.c:160,199,223`, `content_draw.c:12`, `gallery_layout.h:16-34`, `car_records_view.c:15-19` |
| estilos | crema `E8D3B7`, amarillo `FFCE06`, rojo `FF0606`, blanco, tenue `8C8273`; en pantalla (paleta de 256) salen **crema (220,204,180)**, **amarillo (252,204,0)** | `game/sys/text.h:11-17`; medido en `uikit-src-*` |
| rojo de título quemado | (238,0,0) cuerpo, (222,0,0) sombra interior | medido (`cd-options`, `ours-customize`: 238,8,8) |
| rojo de cabecera de tabla | (220,0,0) | medido (`cd-best-times` y `ours-best-times`, idénticos) |
| sombra del texto | negra a +2,+2 | `game/sys/text.h:30` (`text_draw_shadowed`) |
| los títulos del CD | **son dibujo quemado en el fondo**, no letra; los nuestros se recortan de ese dibujo (`title_art`) y se pintan con `text_draw_mask` | `title_art.h:1-8` |

## 2. Catálogo (referencia = el CD; entre paréntesis nuestras pantallas)
### E1. Título de pantalla (rojo quemado) — captura `uikit-titles-burnt-in.png`
- **Medido (CD)**: primera columna **x 9** (Game Mode 10), mayúsculas de **18 filas**: filas 66-83 (Options), 67-84 (Game Mode, Control Central);
  Controls Setup 65-82 (el propio CD no es exacto: ±1-2 px de fila). Letra de serifa, sin recuadro ni subrayado. Color (238,0,0).
- **Código**: `title_art.h:35-44` (`TITLE_ART_X 9`, `TOP 66`, `BASELINE 84`); `central_glue_plus.c:199-210` (`own_title_text`: la máscara recortada; si falta, cae a `text_draw_smooth` a 33/20).
- **Nuestras**: Customize Game 66-83 ✔ (x 9, 18 filas), Graphics Detail 66-83 ✔, Controls Setup 65-82 (igual que el CD). **CONTENT: x 16, filas 71-90 (20 filas)** ✘ (+7 px a la derecha, +5 abajo, +2 de alto): `title_art_for("Content")` no existe (`central_glue_plus.c:172-186`, el enum de `title_art.h:15-22` no lo trae) y se dibuja con la letra a 33/20 en `CONTENT_TITLE_X 14, _Y 62` (`content_nav.h:23`, `content_draw.c:48`).

### E2. Título blanco de las pantallas de pista y galería (con su franja) — fuente `uikit-src-cd-track.png`, `cd-showcase`, `ours-track-page`
- **Medido (CD)**: pista y Best Times: **x 16**, filas **16-40** (25 filas con la sombra), letra blanca itálica con sombra. Car Showcase: x 14, filas 15-39 (nombre) y debajo la franja roja «CAR SHOWCASE».
- **Código**: la galería nuestra usa `GL_NAME_X 12, GL_NAME_Y 8` a 2/1 con una franja desenfocada de `GL_BAND_W 285 × GL_BAND_H 56` (`gallery_layout.h:16-19`); la página de pista, `TITLE_X 12, TITLE_Y 8` con inclinación 2/9 (`track_page_glue.c:46-47,216`).
- **Nuestra página de pista (Dune Run)**: x **20**, filas **21-42** (22 filas). ✘ (+4 px a la derecha, +5 abajo, 3 filas menos que el CD).

### E3. DONE / CANCEL / botón de pie — captura `uikit-footers.png`
- **Medido (CD)**: «DONE» **45 px de ancho y 11 de alto** (cuatro letras, más la sombra), a la **izquierda**, x **7-11** según la pantalla y **y 445-456** (Options, Showcase, Controls) o **449-460** (pista, Best Times); «CANCEL» a la derecha acabando en x 620 (Options) y los botones de pista («Best times», «Description») acabando en x 622-628. Crema con sombra.
- **Código**: el pie es del fondo del CD, no se dibuja; la posición cambia con cada fondo.
- **Nuestras**: Customize Game y Graphics Detail igual que Options (x 10, filas 446-456) ✔; la página de pista igual que la de pista (x 7, 450-460) ✔. **CONTENT** ✘: no usa el pie del CD: «New · Duplicate · Rename · Delete» a la izquierda y «Install… · Done» a la derecha, con **«Done» en el sitio de CANCEL** (x 562-613) y a **4/5** (mayúscula de 8 px frente a 11); `content_draw.c:12,25-26` (`FOOT_X`, `BUTTON_NUM 4/5`).

### E4. Paginador «◀ X ▶» — captura `uikit-pagers.png`
- **Medido (CD)** — pista: flecha izquierda **x 226-234 (9 de ancho), filas 448-464 (17 de alto)**, texto «Road/track» **x 249-372, filas 450-460 (11)**, flecha derecha **x 388-396 (9×17)**. Los huecos texto-flecha son **14 y 15 px** y el centro de la flecha cae **1 px por debajo** del centro del texto (456 frente a 455). Showcase «Next car»: flechas x 225-233 y 380-388, filas 442-458; texto filas 445-455. La **letra** es la del botón (mayúsculas pequeñas negrita, 11 px).
- **Código (hecho)**: `ui_kit.c` `ui_pager` / `ui_page_count` (con `ui_pager_layout` y `ui_arrow_mask`), las medidas en `ui_kit_metrics.h`; `page_nav_draw` (`page_arrows.c`) delega en `ui_page_count`, así que lo toman Controls Setup, Customize Game (Extras), RECORDS y el paginador de Graphics. La caja donde el puntero «está sobre» el paginador sigue en `page_arrows.h` (`PAGE_NAV_*`).
  - **Flechas**: máscara triangular del kit (9×17, sin recortar del CD: no hay píxeles de EA ni dependencia del disco) dibujada por `text_draw_badge_mask` (`text.c`) como las dibuja el CD, medido sobre su volcado de píxeles: relleno crema con grano; **contorno negro puro (0,0,0) de 1 px en las pendientes** (no de 2: un contorno de 8 vecinos engorda las diagonales) y una columna en la base que sobresale 2 filas por cada extremo (40 píxeles en total; el contorno lo trae la propia máscara, `UI_ARROW_OUTLINE`); y **sombra** = la flecha empujada de 1 a 5 px abajo-derecha, oscureciendo un poco más con cada empujón (oscuridad 1-0,8ⁿ: 0,18 / 0,37 / 0,51 / 0,61 / 0,74 medidas en el CD con 1 a 5 empujones; niveles 2/5/7/9/9 de 15 porque la paleta no da más fino).
  - **Recuento «n/m»**: `graver18` a 11/10 (cifras de 11 filas de crema, como las mayúsculas de «Road/track»), por el hueco de texto con escala que cada pantalla trae (`PageNavText`: `plain` sobre su dibujo, `clean` sobre el fondo limpio con el fondo repuesto, y el ancho); sin él, el texto a 1/1 de antes. Las filas 446-456 (el pie de DONE en el fondo de Options) y la flecha una fila por debajo de su centro, como en el CD.
- **Medido después (ctest `ui_kit_check`, `tools/ui_kit_pager_check.py`, página 2/3 de Controls y de Customize)**: flechas **9×17**, huecos **14 y 15**, centro de la flecha **+1** y letras de **11 filas**, las del CD. Antes: 8×15, huecos 18 y 17, centro 0, letras de 10.
- Graphics Detail tiene una sola página: no lleva paginador.

### E5. Selector de campo «◀ valor ▶» — captura `uikit-selectors.png`
- **Medido**: la pista/Best Times usan el pie E4. Game Mode muestra el perfil como nombre grande (letra a **33/20**, `menu_glue_extras.c:82`) con flecha **solo a la derecha** y una línea de descripción debajo; **CONTENT** repite el selector con **flechas a ambos lados**, nombre a otro tamaño y la nota «Read only. Duplicate to edit.» **pegada** debajo del nombre (casi sin hueco), `ARROW_DY 3` (`content_draw.c:12`). Son dos versiones del mismo control en dos pantallas nuestras. Sin medir en cifras (hay que fijarlo en el kit).

### E6. Cabeceras rojas de tabla — captura `uikit-table-headers.png`
- **Medido (CD, Best Times)**: «Player / Time / Car» en rojo (220,0,0), **filas 101-109 (9 de alto)**, columnas en **x 9, 283, 480**, **sin recuadro, línea ni fondo**. En Restomod son **idénticas** (✔): mismas cajas.
- **Código**: Best Times usa el arte del propio fondo (`best_times_pages.c:27`: las cabeceras rojas son del dibujo); `car_records_view.c:15` («HEAD 2/3») las repite pequeñas en la pantalla de récords de coche (**sin medir**: no se pudo capturar).

### E7. Filas de tabla/lista — fuentes `ours-*` y `cd-best-times`
- **Medido (CD, Best Times)**: nombres amarillos, **primera fila en y 122, paso 22 px**, mayúscula de 11, x 9-11; tiempo en x 281, coche en x 481; **el coche en amarillo (252,204,0)**.
- Options (CD): filas **x 8-9, alto 11, paso ≈ 34,5 px** (110, 143, 178, 212, 246, 280, 316, 352, 386); valores a la derecha acabando en x **488-490**.
- Controls Setup (CD): x **17-19** (sangría de sección), alto 10-11, **paso 21-22**; valores acabando en x **597**.
- **Nuestras**: Graphics Detail x **9-11, alto 10-11, paso 25-27 (irregular: 26, 26, 26, 25, 27, 25, 29, 24…)**, valores en 489; Customize Game x **17-19, alto 10, paso 20**, valores en 597; Best Times Restomod ✘ **coche en crema (220,204,180) en vez de amarillo** (y 122-132, x 482-622).

### E8. Etiquetas de baldosa — captura `uikit-tile-labels.png`
- **Medido (CD, Control Central)**: etiqueta roja **dentro** de la baldosa, abajo a la izquierda: jugador **x 25, filas 265-273 (9)**; oponente **x 345, filas 201-209**; localización **x 344, filas 367-375**; tipo de carrera **x 95, filas 367-375**; debajo, la leyenda en crema pequeña («Player», «Opponent», «Location», «Race type»). Tamaño **3/5** (`central_glue_plus.c:160`: `NUM 3, DEN 5, MARGIN 2`).
- **Nuestra** (Control Central con una pista añadida): x **24 / 344 / 345 / 95**, filas **265-273 / 201-209 / 368-375 / 367-375** ✔ (dentro de 1 px).
- **Margen en la galería**: `GALLERY_LABEL_MARGIN 7` (`gallery_label.h:10`, `gallery_label.c`): la etiqueta de la galería nuestra se pone a 7 px del borde y la de Control Central a ~5 (sin recorte): **dos márgenes para el mismo elemento**. La galería de un coche añadido **sin medir** (la captura `added-gallery` salió en negro).

### E9. Título y filas de lista modal (Options/Setup)
- **Medido**: Options del CD = título E1 + 9 filas (E7) + DONE/CANCEL (E3). El panel «Setup» de la galería de coche añadido usa `GL_SETUP_FONT_NUM/DEN 7/8` y `GL_SETUP_LABEL_X 14` «as Options'» (`gallery_layout.h:34`) pero Options mide **mayúscula de 11 px (3/4)**: 7/8 daría ~13. ✘ por código; **sin medir** en captura (galería no capturada).

### E10. Notas pequeñas
- **Medido**: «only when cheat enabled», «only with 360 camera» (Controls Setup) y «Used in: 1 edition», «Read only…» (CONTENT) van a **3/5 de `graver18`** (`content_draw.c:12`: `SMALL 3/5`; `car_records_view.c`). La cara pequeña propia del CD (`MAIN18`, 13 px, `text_small_*`) sólo la usa la página de pista (`track_page_glue.c:483`). Sin alto medido en captura.

### E11. Texto que debe caber (regla `ui_fit`, hecha 2026-11-19)
Todo texto del kit tiene un ancho máximo fijo; el texto libre (el nombre de una edición, los nombres de pista) se ajusta con una de dos reglas, ambas en `ui_kit.h`:
- **CUT**: se corta con «...» (la regla de los nombres y valores de las baldosas del punto 26, `added_card_name_fit`: sin espacio antes de los puntos). No se escribe otra.
- **SHRINK**: baja a la siguiente talla más pequeña del kit; si aún no cabe en la más pequeña, CUT.

| elemento | modo | ancho máximo / tallas |
|---|---|---|
| nombre de la edición en el selector de CONTENT | SHRINK | `UI_SELECTOR_TEXT_W` 230 px; 1/1, 5/6, 3/4 |
| línea «Read only. Duplicate to edit.» | CUT | 230 px; 3/5 |
| valores de las baldosas (General, Mechanical) | CUT (ya así) | el hueco de la baldosa |
| nombres de los coches añadidos en su ficha | CUT (ya así) | el ancho de la ficha |
| nombres de pista en RECORDS | CUT (`fit` de `car_records_view.c` llama a `ui_fit`: antes cortaba con un solo «.») | `NAME_W` 340 px |

Selector «◀ nombre ▶»: los triángulos van **fuera de la línea más ancha** del bloque (nombre o nota) con los huecos del paginador del CD, **14 px a la izquierda y 15 a la derecha** (tinta a tinta; se miden desde el avance del texto, así que el hueco de tinta nunca es menor); el bloque queda centrado (`ui_selector_layout`). `tools/ui_kit_selector_check.py` lo mide sobre CONTENT con un nombre ancho. El nombre de una edición mide como mucho `EDITION_NAME_MAX` = 12 caracteres (`core/formats/edition_ini.h:21`).

## 3. Dónde se desvía hoy cada pantalla nuestra
| pantalla | desviaciones medidas | por código / sin medir |
|---|---|---|
| **Customize Game** (extras) | título ✔; DONE/CANCEL ✔; filas x 17-19 paso 20; paginador ✔ (E4, hecho) | — |
| **Graphics Detail** | título ✔; una sola página, sin paginador; filas con paso irregular 24-29 (E7) | — |
| **Controls Setup** | título ✔ (65-82 como el CD); paginador ✔ (E4, hecho) | — |
| **CONTENT** | **título +7 px / +5 px / +2 filas (letra, no dibujo)**; **DONE a la derecha, a 4/5, «Done» en minúscula-capital**; selector con flechas a ambos lados y nota pegada; **rayas y marco** (regla de pestañas `content_draw.c:62`, división `:63`, pie `:195`, marco de la vista previa `:172-173`) que ningún fondo del CD tiene; tamaños 3/4, 4/5, 3/5 en la misma pantalla | — |
| **Página de pista (añadida)** | título **+4 px / +5 px / −3 filas**; pie ✔ (igual que la pista del CD) | el recuadro de la descripción |
| **Control Central (pista añadida)** | etiquetas ✔ (±1 px) | el fondo/tile vacío gris de la pista sin foto |
| **Best Times (Restomod)** | cabeceras ✔; **coche en crema, no amarillo**; línea de filtro «‹ NORMAL › ·· ‹ NO CHEATS › ·· OPEN ROAD» añadida en la fila del título | — |
| **Galería de coche añadida (RECORDS/Setup)** | **sin capturar** (salió en negro) | margen 7 frente a 5; Setup a 7/8 frente a 3/4 |
| **Récords de coche (RECORDS)** | **sin capturar** (no hay objetivo en `menu_walk.sh`) | cabeceras 2/3, velocidad 3/4, paginador 3/4 (`car_records_view.c:15-19`) |
| **Game Mode (perfil)** | título ✔ | selector con flecha solo a la derecha (E5) |

## 4. Las familias de fallos (lo que el dueño ya ve)
1. **Cada pantalla elige su tamaño de letra**: 3/4, 4/5, 3/5, 2/3, 7/8, 33/20, 3/2, 2/1 (E0, §1). El CD usa **dos**: el botón/fila (mayúscula de 11 px) y la etiqueta pequeña (9 px), más el título quemado (18 px).
2. **El título se cae a la letra** cuando su texto no está en `title_art` (CONTENT): otra posición y otro alto.
3. **El paginador** de las pantallas nuestras no era el de las del CD (flecha 8×15 y hueco 17-18 en vez de 9×17 y 14-15): arreglado en el kit (E4).
4. **Posiciones por pantalla**: DONE y el pie dependen del fondo del CD (y 446/450); cada pantalla nuestra lo coloca a mano (CONTENT lo mueve a la derecha).
5. **Rayas y marcos** (CONTENT) y **columnas con otro color** (Best Times) añadidos sin que el original los tenga.

## 5. Propuesta de API (sin implementar): `game/frontend/ui_kit.c/.h`
**Regla**: una pantalla nueva (o tocada) dibuja **solo** con `ui_*`; ninguna constante de píxel, `num/den` ni llamada a `text_draw*` fuera de `ui_kit.c`. Los valores del §2 viven en un solo sitio (`ui_kit_metrics.h`, con la medida y de dónde sale).
```c
typedef struct UiOps UiOps;          /* las operaciones que ya tiene cada pantalla: texto, relleno, flecha, hueco del puntero */
typedef enum { UI_SIZE_ITEM /*3/4: 11 px*/, UI_SIZE_LABEL /*3/5: 9 px*/, UI_SIZE_NOTE /*MAIN18*/ } UiSize;
typedef enum { UI_BG_OPTIONS /*pie y 446*/, UI_BG_TRACK /*pie y 450*/, UI_BG_SHOWCASE /*pie y 445*/ } UiBackground;

void ui_title(const UiOps *, const char *text);                  /* E1: título quemado a x 9; si no hay letras, lo dice el test, no cae a la letra */
void ui_title_white(const UiOps *, const char *text, int band);  /* E2: x 16, filas 16-40, con su franja */
void ui_done(const UiOps *, UiBackground, int lit);              /* E3: x 7-11, y 446/450/445, 45x11 */
void ui_cancel(const UiOps *, UiBackground, int lit);
void ui_pager(const UiOps *, const char *label, int left, int right, int lit);   /* E4: «◀ label ▶»: cara del botón, flechas 9x17, huecos 14/15, centro +1 */
void ui_page_count(const UiOps *, int page, int pages, int lit); /* E4: lo mismo con «n/m», misma cara y flechas */
void ui_selector(const UiOps *, int x, int y, const char *value, int left, int right);   /* E5: un solo control */
void ui_table_header(const UiOps *, const UiColumn *cols, int n, int y);        /* E6: rojo (220,0,0), 9 px, sin raya */
void ui_table_row(const UiOps *, int row, const UiCell *cells, int n, int selected);   /* E7: paso por clase de tabla, amarillo/crema por columna */
void ui_tile_label(const UiOps *, const UiRect *tile, const char *label, const char *caption);   /* E8: etiqueta roja a 5 px, leyenda crema */
void ui_list_row(const UiOps *, int row, const char *label, const char *value, int selected); /* E9: filas de Options/Setup, x 9, paso de la lista */
void ui_note(const UiOps *, const char *text, int x, int y);     /* E10 */
```
**Comprobación** (para que no se repita): (1) un test de unidad que lea `ui_kit_metrics.h` y falle si alguna constante de `ui_kit.c` no coincide; (2) `tools/ui_kit_check.sh`, que pasa `menu_walk.sh` por cada pantalla y mide la imagen y falla si una caja se desvía más de 1 px de la tabla del §2 (hoy solo el paginador; ctest `ui_kit_check`, que se salta sin el CD ni Pillow); (3) una regla de revisión: un diff que añada `text_draw*`, `NUM/DEN` o `fill(` en `game/frontend/*` fuera de `ui_kit.c` se rechaza.
**Hecho:** E4. **Orden propuesto del resto** (de menor a mayor riesgo): E1 (título: añadir «Content» a `title_art`), E3 (DONE de CONTENT), E7 (columna del coche de Best Times), el resto.

## 6. Sin comprobar / pendiente
- La **galería de un coche añadido** y los **récords de coche** no se pudieron capturar (la primera salió negra; la segunda no tiene objetivo en `menu_walk.sh`): sus medidas son solo de código.
- Los **diálogos modales de CONTENT** (Nuevo, Duplicar, Instalar…) no se capturaron.
- El **paso de fila irregular** de Graphics Detail (24-29) puede ser de la medida (tinta que cae en dos filas): conviene medirlo con una captura de cada fila.
- El alto de las **notas pequeñas** y la **leyenda** de las baldosas (crema atenuado): sin cifra.
- Las medidas son del perfil **Restomod** (los menús no cambian de tamaño con el formato 16:9 salvo el desplazamiento de +107 px).
