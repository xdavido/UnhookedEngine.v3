# Unhooked Engine V3

**Unhooked Engine V3** es la evolución de **Unhooked Engine V2**, es un proyecto correspondiente a la asignatura "Advanced Graphics Programming" del grado en Diseño y Desarrollo de Videojuegos del CITM. 

![Logo](UnhookedEngine_v3/WorkingDir/LogoImage.png)

## 🚀 Descripción del Proyecto

Este motor implementa un **motor de renderizado diferido** basado en los conceptos enseñados en clase. Su objetivo principal es gestionar de manera eficiente una escena 3D compleja con múltiples luces, utilizando un enfoque de renderizado en etapas a través de un **G-buffer**. El sistema permite la visualización interactiva de la escena, con controles en tiempo real para modificar la cámara y las luces, proporcionando una base sólida para entender cómo funcionan los motores gráficos avanzados.

## 🎯 Características Implementadas

### 🕹️ Controles de la Cámara
- **W, A, S, D**: Movimiento de la cámara en el espacio 3D.
- **E, Q**: Movimiento vertical de la cámara (arriba/abajo).
- **SHIFT**: Aumento de velocidad en el movimiento de la cámara.
- **RATÓN (Right Click)**: Rotación de la cámara alrededor de la escena.

### 🏙️ Escena 3D
- La escena está compuesta por modelos 3D estáticos, creados y configurados en la función **Init()**.
- No se incluye carga dinámica de modelos, ya que los objetos son definidos y cargados al inicio del programa.

### 🎨 Visualización G-Buffer y Framebuffer
- Puedes seleccionar entre visualizar el **G-Buffer** o el **Framebuffer** para inspeccionar el contenido del renderizado.
- El motor usa un **Framebuffer Object (FBO)** con múltiples **render targets** (texturas) para almacenar las propiedades del material de la escena, como:
  - **Albedo** (color base)
  - **Normales** (dirección de las normales)
  - **Posición** (coordenadas espaciales)
  - **ViewDir** (dirección de la vista)
  - **Depth** (profundidad de la escena)

### 💡 Sistema de Iluminación
El motor permite probar distintos tipos de luces, tanto estáticas como dinámicas:
- **Luces Direccionales**: Simuladas como quads renderizados en pantalla completa, proporcionando iluminación global.
- **Luces Puntuales**: Representadas como esferas distribuidas por la escena, afectando localmente a los objetos cercanos.

Ambos tipos de luz se configuran inicialmente en la función **Init()**, pero pueden ser editados y modificados en tiempo real utilizando **ImGui** 🛠️.


