# Voxel GI prototype

The indoor scene voxelizes the static `Library` glTF model into one 64³
`R16G16B16A16_FLOAT` volume. The voxel pixel shader writes opacity and
approximate outgoing radiance from the directional light. Mipmaps are generated
after voxelization. The deferred lighting shader traces five cones per visible
pixel and adds the result to diffuse lighting.

Open the **Voxel GI** section in the indoor scene debug UI to adjust strength
or enable **Show volume slice**. The `Y slice` control displays a horizontal
cross-section of occupied voxels. Disable the existing SSGI effect when
comparing the contribution of Voxel GI alone.

The volume updates every fourth frame, or every frame while the slice preview
is open. Only the static Library model contributes. The voxel lighting currently
uses the directional light and a small ambient term; it does not sample shadow
maps, include point or spot lights, or perform further light bounces. The
64³ grid and standard mip filtering can leak light through thin surfaces.
