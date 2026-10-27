# Assets 3D de la academia

Los modelos son originales de este proyecto. Se generaron con Blender, sin modelos, texturas ni dependencias descargadas. La dirección artística es estilizada: anatomía simplificada, ropa de academia, piedra biselada, bronce y luz turquesa. No son personajes fotorealistas ni retratos de personas reales.

## Inventario

| Archivo en `assets/models/` | Personaje / módulo |
| --- | --- |
| `professor.glb` | Profesora Elena, recogido castaño, blusa azul con cuello y puños blancos, pantalón negro, aretes azules y libros rojo/verde; basada en el avatar aportado por el usuario |
| `student_f01.glb` | Lucía, cabello largo, chaqueta turquesa |
| `student_f02.glb` | Valeria, corte bob, chaqueta lavanda |
| `student_f03.glb` | Camila, cola de caballo, chaqueta ocre |
| `student_m01.glb` | Mateo, rizos, chaqueta roja |
| `student_m02.glb` | Diego, cabello de lado, chaqueta azul |
| `student_m03.glb` | Andrés, cabello corto, chaqueta verde |
| `floor.glb` | Loseta de piedra de 3 × 3 m con borde de bronce |
| `wall.glb` | Pared de 3 m en X × 1.35 m de alto, centrada en X/Z |
| `podium.glb` | Podio circular de tres niveles, diámetro 2.1 m, altura 0.64 m |
| `glyph.glb` | Cristal luminoso con halo flotante, centrado en el origen |
| `pillar.glb` | Columna con farol, altura aproximada de 1.91 m |

Los GLB utilizan **Y arriba**, personajes mirando hacia **+Z**, metros, pies cerca de Y=0. Los personajes miden aproximadamente 1.85 m. El piso tiene su cara superior cerca de Y=0; los demás módulos se apoyan en Y=0. `wall.glb` se puede rotar 90° alrededor de Y para cerrar las paredes del otro eje.

Cada personaje incluye una malla con grupos de materiales, un esqueleto de 16 huesos y tres animaciones: `Idle`, `Walk`, `Celebrate`. La animación de caminata es en el lugar: la lógica del juego mueve al personaje. Las prendas, extremidades y accesorios usan pesos rígidos por hueso; el rig conserva articulaciones y piezas editables, aunque no implementa deformación muscular ni expresiones faciales. El GLB transporta el material emisivo del glifo; la apariencia luminosa final también depende del shader del motor.

La profesora se adaptó al avatar de referencia compartido: sin lentes, expresión sonriente, ropa azul/blanca y dos libros sujetos al hueso `Hand.L`. Los libros se mueven con el brazo en los tres clips y forman parte del modelo; no necesitan una entidad adicional. Su clip `Idle` incorpora un gesto abierto de enseñanza con el brazo libre.

## Regenerar y editar

Desde la raíz del repositorio, en macOS:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/generate_assets.py
python3 tools/validate_assets.py
```

Para omitir los renders y regenerar solo los modelos y la fuente:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/generate_assets.py -- --no-render
```

Para exportar únicamente el GLB de la profesora y su vista individual, preservando los otros once GLB (la fuente editable completa se reconstruye):

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/generate_assets.py -- --only-professor
```

El generador sobrescribe sus propios archivos de salida. **Guardar las modificaciones manuales en otro `.blend` antes de regenerar.** La ejecución requiere acceso al sistema gráfico de macOS incluso en modo background; Blender puede fallar al iniciar Metal dentro de un sandbox sin ese acceso.

- `assets/source/academy.blend`: escena editable con los siete personajes, rigs, pistas NLA y módulos. Los módulos están detrás de la galería, ocultos del render; se pueden seleccionar en el Outliner.
- `assets/source/manifest.json`: inventario de modelos y animaciones.
- `assets/previews/academy_cast.png`: presentación de los siete personajes.
- `assets/previews/academy_celebrate.png`: pose de celebración para revisar articulaciones.
- `assets/previews/professor_reference.png`: vista actual de la profesora adaptada al avatar; las vistas de grupo anteriores conservan su diseño inicial hasta regenerar todos los renders.

Las pistas NLA se llaman igual que los clips exportados. Para previsualizar una animación en Blender, activar una pista y silenciar las otras. Se usan 30 fps en la fuente; raylib vuelve a muestrear las curvas al cargar el GLB. La duración debe calcularse con la frecuencia del importador, no suponiendo que la cantidad de keyframes del GLB equivale a los cuadros de Blender.

Blender es una herramienta de autoría; **no hace falta instalarlo para compilar ni jugar**. Los archivos `assets/models/*.glb` ya están exportados. El proyecto controla la carga y el tiempo de vida de esos recursos mediante `ResourceManager`.
