#pragma once
#include "math_util.h"

namespace Transform_Util
{
	/**
	 * @brief スクリーン座標系 (左上原点, ピクセル単位) から NDC (正規化デバイス座標系) へ変換します
	 *
	 * NDC座標系は DirectX において [-1, 1] の範囲で表されます。
	 * スクリーン座標の Y 軸は上方向が負になるため、変換時に反転処理を行います。
	 *
	 * @param x スクリーン座標のX値（ピクセル単位）
	 * @param y スクリーン座標のY値（ピクセル単位）
	 * @param screenWidth スクリーンの幅（ピクセル数）
	 * @param screenHeight スクリーンの高さ（ピクセル数）
	 * @return dx::XMVECTOR NDC座標 (x, y, 0, 1)
	 */
	inline dx::XMVECTOR screen_to_NDC(float x, float y, float screenWidth, float screenHeight) {
		// NDC座標系は[-1, 1]の範囲
		float ndcX = (2.0f * x / screenWidth) - 1.0f;
		float ndcY = 1.0f - (2.0f * y / screenHeight); // Y軸は反転
		return dx::XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
	}

	/**
	 * @brief NDC (正規化デバイス座標系) からスクリーン座標系 (左上原点, ピクセル単位) へ変換します
	 *
	 * NDC座標系 [-1, 1] をスクリーンの解像度にマッピングします。
	 * Y軸はスクリーン座標系で上方向が正となるため、変換時に反転処理を行います。
	 *
	 * @param ndcX NDC座標のX値（-1 ～ 1）
	 * @param ndcY NDC座標のY値（-1 ～ 1）
	 * @param screenWidth スクリーンの幅（ピクセル数）
	 * @param screenHeight スクリーンの高さ（ピクセル数）
	 * @return dx::XMVECTOR スクリーン座標 (x, y, 0, 1)
	 */
	inline dx::XMVECTOR NDC_to_screen(float ndcX, float ndcY, float screenWidth, float screenHeight) {
		// スクリーン座標系は左上原点
		float x = (ndcX + 1.0f) * 0.5f * screenWidth;
		float y = (1.0f - ndcY) * 0.5f * screenHeight; // Y軸は反転
		return dx::XMVectorSet(x, y, 0.0f, 1.0f);
	}

} // namespace transform_util
