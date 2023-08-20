## Vulkan ReSTIR PT

This is another Vulkan ReSTIR ray tracer project, aiming for rebuilding [*EIDOLA*](https://github.com/IwakuraRein/CIS-565-Final-VR-Raytracer) completely from scratch and adding *ReSTIR PT (GRIS)* for indirect illumination.

### Contributor

- Chang Liu

### Progress

Just finished the base framework of a Vulkan ray tracer, which took much longer than I expected. Vulkan is tough :( but got it working.

### TODO

- Setting up light source data structure
- Monte Carlo ray tracing shader code
- ReSTIR DI
- ReSTIR PT
- Batched G-buffer draw call
- Cache & memory improvement as in RTXDI
- Async command recording & submission
- Denoiser?
