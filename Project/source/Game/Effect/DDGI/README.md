# Voxel-guided DDGI probe field

The indoor and Voxel GI Emission scenes keep a configurable probe grid inside
the static voxel volume (8 x 8 x 8 by default). A compute pass refreshes one eighth of the probes per
frame. Each refreshed probe traces 64 directions through the 64³ voxel volume,
accumulates six diffuse irradiance lobes, and stores hit-distance moments.
The rays rotate on subsequent refreshes so hysteresis integrates different
directions. A separate pass smooths neighboring irradiance only when the
voxel path between probes is clear; raw probe history and visibility moments
remain unchanged. Probe lookup uses trilinear weights, a normal-facing weight,
and a directionally blended Chebyshev visibility estimate. Rebuilding the
voxel volume resets the probe history and fills it again over eight frames.
Surface lookup is offset toward the normal by 1.5 voxel cells. The probe
visibility used to average irradiance also controls the blend back to voxel
GI, so a partly occluded probe cannot suddenly dominate the interpolated
result at a grid boundary.

At each voxel hit, the update evaluates the current directional, point, and
spot lights against the stored material albedo and normal. Area lights use a compact
center-and-size approximation. Light-to-surface shadows are not evaluated in
this pass; voxel ray visibility is used for probe-to-surface and
probe-to-receiver paths.

The deferred pass blends probe irradiance with voxel cone tracing rather than
adding both estimates. SSGI runs afterward and adds screen-space detail in
proportion to its valid sample weight. The **Voxel GI Emission** test scene
starts with SSGI disabled; enable it in Rendering to compare all three.
The volumetric fog injection pass also samples the filtered probes inside the
static GI volume. It adds isotropic indirect scattering alongside the existing
direct light shafts and skips fog density in occupied voxels. The **GI
scattering** control in Volumetric Fog scales this contribution; the UI shows
whether the DDGI input is active. **Show GI fog only** removes direct light
from the fog injection and leaves scene transmittance unchanged for comparison.
The contribution is zero when DDGI is
unavailable or disabled.

The implementation follows the irradiance-field and visibility-moment ideas
from [Majercik et al., JCGT 2019](https://jcgt.org/published/0008/02/01/) and
NVIDIA's [RTXGI DDGI documentation](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/docs/DDGIVolume.md).
It uses D3D11 voxel ray marching, not hardware ray tracing. Probe radiance
combines the static voxel bake (emissive materials and an environment fill)
with current scene lights. Moving geometry is not voxelized. The six-lobe
representation is lower resolution than a full RTXGI implementation.

**Show probes in 3D** draws translucent wireframe markers after post processing;
the markers do not change scene color or depth used by fog. Cyan groups
have updated and orange groups are pending. Set **Probe grid XYZ** from 2 to
12 per axis and press **Apply probe grid** to recreate the buffers. The field
fills over eight frames. **Inject scene lights** isolates the lighting
contribution when comparing probe behavior.
