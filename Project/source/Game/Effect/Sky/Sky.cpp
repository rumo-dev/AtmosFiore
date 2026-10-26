#include "Sky.h"
#include "Engine/system/render_state.h"
#include "Engine/System/Manager/resource_manager.h"
#include "tracy_util.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
	uint32_t noise_hash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed)
	{
		uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ z * 0xcb1ab31fu ^ seed * 0x165667b1u;
		h ^= h >> 16;
		h *= 0x7feb352du;
		h ^= h >> 15;
		h *= 0x846ca68bu;
		h ^= h >> 16;
		return h;
	}

	uint16_t float_to_half(float value)
	{
		uint32_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		const uint32_t sign = (bits >> 16) & 0x8000u;
		int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;
		uint32_t mantissa = bits & 0x7fffffu;
		if (exponent <= 0)
		{
			if (exponent < -10) return static_cast<uint16_t>(sign);
			mantissa = (mantissa | 0x800000u) >> (1 - exponent);
			return static_cast<uint16_t>(sign | ((mantissa + 0x1000u) >> 13));
		}
		if (exponent >= 31) return static_cast<uint16_t>(sign | 0x7c00u);
		return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | ((mantissa + 0x1000u) >> 13));
	}

	std::array<float, 4> integrate_transmittance(float height, float cosine_zenith)
	{
		constexpr float planet_radius = 6360000.0f;
		constexpr float atmosphere_height = 100000.0f;
		constexpr int steps = 32;
		const float radius = planet_radius + std::clamp(height, 0.0f, atmosphere_height);
		const float top_radius = planet_radius + atmosphere_height;
		if (cosine_zenith < 0.0f && height <= 1.0e-3f) return { 0.0f, 0.0f, 0.0f, 0.0f };
		const float discriminant = radius * radius * (cosine_zenith * cosine_zenith - 1.0f) + top_radius * top_radius;
		if (discriminant <= 0.0f) return { 0.0f, 0.0f, 0.0f, 0.0f };
		float distance = -radius * cosine_zenith + std::sqrt(discriminant);
		if (cosine_zenith < 0.0f)
		{
			const float ground_discriminant = radius * radius * (cosine_zenith * cosine_zenith - 1.0f) + planet_radius * planet_radius;
			if (ground_discriminant >= 0.0f)
			{
				const float ground_distance = -radius * cosine_zenith - std::sqrt(ground_discriminant);
				if (ground_distance > 0.0f && ground_distance < distance) return { 0, 0, 0, 0 };
			}
		}
		const float step = distance / steps;
		float optical_rayleigh = 0.0f, optical_mie = 0.0f, optical_ozone = 0.0f;
		for (int i = 0; i < steps; ++i)
		{
			const float t = (i + 0.5f) * step;
			const float altitude = (std::max)(std::sqrt(radius * radius + t * t + 2.0f * radius * cosine_zenith * t) - planet_radius, 0.0f);
			optical_rayleigh += std::exp(-altitude / 8000.0f) * step;
			optical_mie += std::exp(-altitude / 1200.0f) * step;
			const float ozone = (std::max)(1.0f - std::abs(altitude - 25000.0f) / 15000.0f, 0.0f);
			optical_ozone += ozone * step;
		}
		return {
			std::exp(-5.8e-6f * optical_rayleigh - 2.1e-5f * optical_mie - 6.0e-7f * optical_ozone),
			std::exp(-13.5e-6f * optical_rayleigh - 2.1e-5f * optical_mie - 6.0e-7f * optical_ozone),
			std::exp(-33.1e-6f * optical_rayleigh - 2.1e-5f * optical_mie - 6.0e-7f * optical_ozone),
			1.0f
		};
	}

	template <typename Texture>
	bool create_immutable_lut2d(ID3D11Device* device, uint32_t width, uint32_t height,
		const std::vector<std::array<float, 4>>& values, Texture& texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
	{
		std::vector<uint16_t> data(values.size() * 4);
		for (size_t i = 0; i < values.size(); ++i)
			for (size_t c = 0; c < 4; ++c)
			data[i * 4 + c] = float_to_half(values[i][c]);
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = data.data(); initial.SysMemPitch = width * 4u * sizeof(uint16_t);
		initial.SysMemSlicePitch = initial.SysMemPitch * height;
		return SUCCEEDED(device->CreateTexture2D(&desc, &initial, texture.GetAddressOf())) &&
			SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()));
	}

	bool create_immutable_lut3d(ID3D11Device* device, uint32_t size,
		const std::vector<std::array<float, 4>>& values, Microsoft::WRL::ComPtr<ID3D11Texture3D>& texture,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
	{
		std::vector<uint16_t> data(values.size() * 4);
		for (size_t i = 0; i < values.size(); ++i)
			for (size_t c = 0; c < 4; ++c)
			data[i * 4 + c] = float_to_half(values[i][c]);
		D3D11_TEXTURE3D_DESC desc{};
		desc.Width = desc.Height = desc.Depth = size; desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = data.data(); initial.SysMemPitch = size * 4u * sizeof(uint16_t);
		initial.SysMemSlicePitch = initial.SysMemPitch * size;
		return SUCCEEDED(device->CreateTexture3D(&desc, &initial, texture.GetAddressOf())) &&
			SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()));
	}

	float hash_unit(uint32_t x, uint32_t y, uint32_t z, uint32_t seed)
	{
		return static_cast<float>(noise_hash(x, y, z, seed) & 0x00ffffffu) / 16777216.0f;
	}

	std::vector<std::array<float, 4>> make_worley_volume(uint32_t size, const std::array<uint32_t, 4>& frequencies, uint32_t seed)
	{
		const size_t voxel_count = static_cast<size_t>(size) * size * size;
		std::vector<std::array<float, 4>> volume(voxel_count);
		for (uint32_t channel = 0; channel < frequencies.size(); ++channel)
		{
			const uint32_t frequency = frequencies[channel];
			std::vector<std::array<float, 3>> feature_points(static_cast<size_t>(frequency) * frequency * frequency);
			for (uint32_t z = 0; z < frequency; ++z)
			for (uint32_t y = 0; y < frequency; ++y)
			for (uint32_t x = 0; x < frequency; ++x)
			{
				const size_t cell_index = (static_cast<size_t>(z) * frequency + y) * frequency + x;
				feature_points[cell_index] = {
					hash_unit(x, y, z, seed + channel * 11u),
					hash_unit(x, y, z, seed + channel * 11u + 1u),
					hash_unit(x, y, z, seed + channel * 11u + 2u)
				};
			}
			std::vector<float> distances(voxel_count);
			float minimum = 1.0e9f;
			float maximum = 0.0f;
			for (uint32_t z = 0; z < size; ++z)
			for (uint32_t y = 0; y < size; ++y)
		for (uint32_t x = 0; x < size; ++x)
			{
				const float px = (static_cast<float>(x) + 0.5f) * frequency / size;
				const float py = (static_cast<float>(y) + 0.5f) * frequency / size;
				const float pz = (static_cast<float>(z) + 0.5f) * frequency / size;
				const int32_t cx = static_cast<int32_t>(std::floor(px));
				const int32_t cy = static_cast<int32_t>(std::floor(py));
				const int32_t cz = static_cast<int32_t>(std::floor(pz));
				float nearest_squared = 1.0e9f;
				for (int32_t oz = -1; oz <= 1; ++oz)
				for (int32_t oy = -1; oy <= 1; ++oy)
				for (int32_t ox = -1; ox <= 1; ++ox)
				{
					const int32_t raw_x = cx + ox, raw_y = cy + oy, raw_z = cz + oz;
					const uint32_t cell_x = static_cast<uint32_t>((raw_x % static_cast<int32_t>(frequency) + frequency) % frequency);
					const uint32_t cell_y = static_cast<uint32_t>((raw_y % static_cast<int32_t>(frequency) + frequency) % frequency);
					const uint32_t cell_z = static_cast<uint32_t>((raw_z % static_cast<int32_t>(frequency) + frequency) % frequency);
					const size_t cell_index = (static_cast<size_t>(cell_z) * frequency + cell_y) * frequency + cell_x;
					const auto& feature = feature_points[cell_index];
					const float dx = static_cast<float>(ox) + feature[0] - (px - cx);
					const float dy = static_cast<float>(oy) + feature[1] - (py - cy);
					const float dz = static_cast<float>(oz) + feature[2] - (pz - cz);
				nearest_squared = (std::min)(nearest_squared, dx * dx + dy * dy + dz * dz);
				}
				const size_t index = (static_cast<size_t>(z) * size + y) * size + x;
				distances[index] = std::sqrt(nearest_squared);
				minimum = (std::min)(minimum, distances[index]);
				maximum = (std::max)(maximum, distances[index]);
			}

			const float range = (std::max)(maximum - minimum, 1.0e-5f);
			for (size_t i = 0; i < voxel_count; ++i)
				volume[i][channel] = 1.0f - (distances[i] - minimum) / range;
		}
		return volume;
	}

	bool create_worley_texture(ID3D11Device* device, uint32_t size, const std::array<uint32_t, 4>& frequencies, uint32_t seed,
		Microsoft::WRL::ComPtr<ID3D11Texture3D>& texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
	{
		const auto values = make_worley_volume(size, frequencies, seed);
		std::vector<uint16_t> half_values(values.size() * 4);
		for (size_t i = 0; i < values.size(); ++i)
			for (size_t c = 0; c < 4; ++c)
			half_values[i * 4 + c] = float_to_half(values[i][c]);

		D3D11_TEXTURE3D_DESC desc{};
		desc.Width = desc.Height = desc.Depth = size;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA initial{};
		initial.pSysMem = half_values.data();
		initial.SysMemPitch = size * 4u * sizeof(uint16_t);
		initial.SysMemSlicePitch = initial.SysMemPitch * size;
		return SUCCEEDED(device->CreateTexture3D(&desc, &initial, texture.GetAddressOf())) &&
			SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()));
	}
}

Sky::Sky(ID3D11Device* device, uint32_t width, uint32_t height)
{
	TRACY_CPU_ZONE_C("Sky::Sky", TracyCategory::System);

	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	color_map = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);
	HRESULT hr{ S_OK };
	// 定数バッファの作成
	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.ByteWidth = sizeof(Constants);
	hr = device->CreateBuffer(&cb_desc, nullptr, constants.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateBuffer failed");

	create_atmosphere_luts(device);
	create_cloud_noise_textures(device);
}

void Sky::create_cloud_noise_textures(ID3D11Device* device)
{
	// Two seamless, inverted Worley volumes. Their RGBA channels hold four
	// normalized frequency bands, matching the paper's base/detail representation.
	const bool base_ok = create_worley_texture(device, 64, { 4, 8, 16, 32 }, 17u, cloud_base_noise, cloud_base_noise_srv);
	const bool detail_ok = create_worley_texture(device, 32, { 4, 8, 16, 32 }, 71u, cloud_detail_noise, cloud_detail_noise_srv);
	assert(base_ok && detail_ok && "Cloud Worley texture creation failed");
}

void Sky::create_atmosphere_luts(ID3D11Device* device)
{
	// Hillaire-style atmosphere tables: transmittance, a compact isotropic
	// multiple-scattering approximation, sky view, and aerial perspective.
	std::vector<std::array<float, 4>> transmittance(256u * 64u);
	for (uint32_t y = 0; y < 64; ++y)
	for (uint32_t x = 0; x < 256; ++x)
	{
		const float height = static_cast<float>(y) / 63.0f * 80000.0f;
		const float mu = static_cast<float>(x) / 255.0f * 2.0f - 1.0f;
		transmittance[static_cast<size_t>(y) * 256 + x] = integrate_transmittance(height, mu);
	}
	const bool trans_ok = create_immutable_lut2d(device, 256, 64, transmittance, transmittance_lut, transmittance_lut_srv);
	assert(trans_ok && "Transmittance LUT creation failed");

	std::vector<std::array<float, 4>> multiple(32u * 32u);
	for (uint32_t y = 0; y < 32; ++y)
	for (uint32_t x = 0; x < 32; ++x)
	{
		const float height = static_cast<float>(y) / 31.0f * 80000.0f;
		const float mu = static_cast<float>(x) / 31.0f * 2.0f - 1.0f;
		const float daylight = std::clamp((mu + 0.05f) / 0.15f, 0.0f, 1.0f);
		std::array<float, 3> second_order{};
		float average_scattering_fraction = 0.0f;
		constexpr int direction_count = 64;
		constexpr int ray_steps = 8;
		for (int direction = 0; direction < direction_count; ++direction)
		{
			const float view_mu = 1.0f - 2.0f * (direction + 0.5f) / direction_count;
			const float ray_length = (std::min)(80000.0f / (std::max)(std::abs(view_mu), 0.12f), 100000.0f);
			const float step = ray_length / ray_steps;
			float optical_r = 0.0f, optical_m = 0.0f;
			for (int step_index = 0; step_index < ray_steps; ++step_index)
			{
				const float distance = (step_index + 0.5f) * step;
				const float altitude = (std::max)(height + distance * view_mu + distance * distance / (2.0f * 6360000.0f), 0.0f);
				const float density_r = std::exp(-altitude / 8000.0f);
				const float density_m = std::exp(-altitude / 1200.0f);
				const auto sun_t = integrate_transmittance(altitude, mu);
				const float view_r = std::exp(-5.8e-6f * optical_r);
				const float view_g = std::exp(-13.5e-6f * optical_r);
				const float view_b = std::exp(-33.1e-6f * optical_r);
				const float view_m = std::exp(-2.1e-5f * optical_m);
				second_order[0] += (density_r * 5.8e-6f * sun_t[0] * view_r + density_m * 2.1e-5f * sun_t[0] * view_m) * step;
				second_order[1] += (density_r * 13.5e-6f * sun_t[1] * view_g + density_m * 2.1e-5f * sun_t[1] * view_m) * step;
				second_order[2] += (density_r * 33.1e-6f * sun_t[2] * view_b + density_m * 2.1e-5f * sun_t[2] * view_m) * step;
				optical_r += density_r * step;
				optical_m += density_m * step;
			}
			average_scattering_fraction += (1.0f - std::exp(-2.1e-5f * optical_m)) / direction_count;
		}
		const float geometric_gain = 1.0f / (std::max)(1.0f - average_scattering_fraction * 0.5f, 0.25f);
		multiple[static_cast<size_t>(y) * 32 + x] = {
			second_order[0] / direction_count * geometric_gain * daylight,
			second_order[1] / direction_count * geometric_gain * daylight,
			second_order[2] / direction_count * geometric_gain * daylight,
			1.0f
		};
	}
	const bool multi_ok = create_immutable_lut2d(device, 32, 32, multiple, multiscattering_lut, multiscattering_lut_srv);
	assert(multi_ok && "Multiscattering LUT creation failed");

	std::vector<std::array<float, 4>> sky_view(192u * 128u);
	for (uint32_t y = 0; y < 128; ++y)
	for (uint32_t x = 0; x < 192; ++x)
	{
		const float sun = -0.2f + (static_cast<float>(y) / 127.0f) * 1.2f;
		const float sun_visibility = std::clamp((sun + 0.14f) / 0.17f, 0.0f, 1.0f);
		const float sun_cosine_floor = (std::max)(sun + 0.15f, 0.06f);
		const float air_mass = 1.0f / sun_cosine_floor;
		const float sun_depth_r = 8000.0f * air_mass;
		const float sun_depth_m = 1200.0f * air_mass;
		const float raw_elev = -0.08f + (static_cast<float>(x) / 191.0f) * 1.08f;
		const float elev = (std::max)(raw_elev, 0.0f);
		const float view_length = (std::min)(8000.0f / (std::max)(elev, 0.1f), 80000.0f);
		const float step_length = view_length / 24.0f;
		float depth_r = 0.0f, depth_m = 0.0f, scatter_r = 0.0f, scatter_m = 0.0f;
		for (int sample = 0; sample < 24; ++sample)
		{
			const float distance = (sample + 0.5f) * step_length;
			const float altitude = distance * elev + distance * distance / (2.0f * 6360000.0f);
			const float density_r = std::exp(-altitude / 8000.0f);
			const float density_m = std::exp(-altitude / 1200.0f);
			const float tr = std::exp(-5.8e-6f * (depth_r + sun_depth_r));
			const float tg = std::exp(-13.5e-6f * (depth_r + sun_depth_r));
			const float tb = std::exp(-33.1e-6f * (depth_r + sun_depth_r));
			const float tm = std::exp(-2.1e-5f * (depth_m + sun_depth_m));
			scatter_r += density_r * step_length * (tr + tg + tb) / 3.0f * sun_visibility;
			scatter_m += density_m * step_length * tm * sun_visibility;
			depth_r += density_r * step_length;
			depth_m += density_m * step_length;
		}
		sky_view[static_cast<size_t>(y) * 192 + x] = {
			scatter_r * 5.8e-6f * 20.0f,
			scatter_r * 13.5e-6f * 20.0f,
			scatter_r * 33.1e-6f * 20.0f,
			scatter_m * 2.1e-5f * 20.0f
		};
	}
	const bool sky_ok = create_immutable_lut2d(device, 192, 128, sky_view, atmosphere_lut, atmosphere_lut_srv);
	assert(sky_ok && "Sky-view LUT creation failed");

	constexpr uint32_t aerial_size = 32;
	std::vector<std::array<float, 4>> aerial(aerial_size * aerial_size * aerial_size);
	for (uint32_t z = 0; z < aerial_size; ++z)
	for (uint32_t y = 0; y < aerial_size; ++y)
	for (uint32_t x = 0; x < aerial_size; ++x)
	{
		const float view_elevation = static_cast<float>(x) / (aerial_size - 1) * 2.0f - 1.0f;
		const float sun_elevation = -0.2f + static_cast<float>(y) / (aerial_size - 1) * 1.2f;
		const float distance = static_cast<float>(z) / (aerial_size - 1) * 80000.0f;
		const float step = distance / 16.0f;
		float optical_r = 0.0f, optical_m = 0.0f;
		std::array<float, 3> radiance{};
		for (int i = 0; i < 16; ++i)
		{
			const float t = (i + 0.5f) * step;
			const float height = (std::max)(t * view_elevation, 0.0f);
			const float density_r = std::exp(-height / 8000.0f);
			const float density_m = std::exp(-height / 1200.0f);
			const auto sun_t = integrate_transmittance(height, sun_elevation);
			const float view_r = std::exp(-5.8e-6f * optical_r);
			const float view_g = std::exp(-13.5e-6f * optical_r);
			const float view_b = std::exp(-33.1e-6f * optical_r);
			const float view_m = std::exp(-2.1e-5f * optical_m);
			radiance[0] += (density_r * 5.8e-6f * sun_t[0] * view_r + density_m * 2.1e-5f * sun_t[0] * view_m) * step * 0.2f;
			radiance[1] += (density_r * 13.5e-6f * sun_t[1] * view_g + density_m * 2.1e-5f * sun_t[1] * view_m) * step * 0.2f;
			radiance[2] += (density_r * 33.1e-6f * sun_t[2] * view_b + density_m * 2.1e-5f * sun_t[2] * view_m) * step * 0.2f;
			optical_r += density_r * step;
			optical_m += density_m * step;
		}
		const float tr = std::exp(-5.8e-6f * optical_r - 2.1e-5f * optical_m);
		const size_t index = (static_cast<size_t>(z) * aerial_size + y) * aerial_size + x;
		aerial[index] = { radiance[0], radiance[1], radiance[2], tr };
	}
	const bool aerial_ok = create_immutable_lut3d(device, aerial_size, aerial, aerial_perspective_lut, aerial_perspective_lut_srv);
	assert(aerial_ok && "Aerial-perspective LUT creation failed");
}

void Sky::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv)
{
	TRACY_CPU_ZONE_C("Sky::make", TracyCategory::RenderPass);
	TRACY_GPU_ZONE_C("Sky::make", TracyCategory::RenderPass);

	// パイプラインの衝突を防ぐため、最初にすべての関連スロットを一度安全にクリアする
	ID3D11ShaderResourceView* null_srvs[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	immediate_context->PSSetShaderResources(0, 8, null_srvs);
	Render_State::instance().set_2d_render_states(immediate_context);

	// 1. 定数バッファの更新
	{
		TRACY_CB_ZONE("Sky::make::UpdateConstants");

		Constants cb_data{};
		cb_data.gTime = time;
		immediate_context->UpdateSubresource(constants.Get(), 0, 0, &cb_data, 0, 0);
		immediate_context->PSSetConstantBuffers(8, 1, constants.GetAddressOf());
	}

	{
		TRACY_CPU_ZONE_N("Sky::make::SkyPass");

		color_map->Clear(immediate_context, 0, 0, 0, 1);
		color_map->Activate(immediate_context);
		{
			ID3D11ShaderResourceView* srvs[8] = {
				input_hdr_srv,
				Graphics_Core::instance().get_geometry_buffer()->GetDepth(),
				atmosphere_lut_srv.Get(),
				cloud_base_noise_srv.Get(),
				cloud_detail_noise_srv.Get(),
				transmittance_lut_srv.Get(),
				multiscattering_lut_srv.Get(),
				aerial_perspective_lut_srv.Get()
			};
			bit_block_transfer->Blit(immediate_context, srvs, 0, 8, Resource_Manager::instance().get_native_shader<Pixel_Shader>("SKY_PS"));
		}
		// Release G-buffer depth before restoring it as the previous depth target.
		immediate_context->PSSetShaderResources(0, 8, null_srvs);
		color_map->Deactivate(immediate_context);
	}
}
