#ifndef __SAMPLERS_H__
#define __SAMPLERS_H__

/**
 * @file samplers.h
 * @brief サンプラステート識別子定義
 *
 * @details
 * シェーダーおよびC++側で共通利用するサンプラステートの
 * インデックスを定義する。
 *
 * @note
 * ・配列インデックスとして直接使用する軽量設計
 * ・enum ではなく #define を使うことで HLSL との共有を容易にしている
 * ・Render_State::_sampler_states[] と対応している必要がある
 *
 * @warning
 * ・単なる整数マクロのため型安全ではない
 * ・範囲外アクセスしてもコンパイルエラーにならない
 * ・順番変更は Render_State 側と完全同期が必要（非常に重要）
 */

 /** @name Wrap サンプラ
  *  テクスチャ座標を繰り返す
  *  @{
  */
#define POINT_WRAP 0               ///< 最近傍補間 + Wrap
#define LINEAR_WRAP 1              ///< 線形補間 + Wrap
#define ANISOTROPIC_WRAP 2         ///< 異方性フィルタ + Wrap
  /** @} */

  /** @name Clamp サンプラ
   *  テクスチャ端で固定
   *  @{
   */
#define POINT_CLAMP 3              ///< 最近傍補間 + Clamp
#define LINEAR_CLAMP 4             ///< 線形補間 + Clamp
#define ANISOTROPIC_CLAMP 5        ///< 異方性フィルタ + Clamp
   /** @} */

   /** @name Border（黒）
	*  範囲外は黒で補完
	*  @{
	*/
#define POINT_BORDER_OPAQUE_BLACK 6   ///< 最近傍 + 黒ボーダー
#define LINEAR_BORDER_OPAQUE_BLACK 7  ///< 線形 + 黒ボーダー
	/** @} */

	/** @name Border（白）
	 *  範囲外は白で補完
	 *  @{
	 */
#define POINT_BORDER_OPAQUE_WHITE 8   ///< 最近傍 + 白ボーダー
#define LINEAR_BORDER_OPAQUE_WHITE 9  ///< 線形 + 白ボーダー
	 /** @} */

	 /** @name Mirror
	  *  ミラーリング繰り返し
	  *  @{
	  */
#define POINT_MIRROR 10           ///< 最近傍 + Mirror
#define LINEAR_MIRROR 11          ///< 線形 + Mirror
#define ANISOTROPIC_MIRROR 12     ///< 異方性 + Mirror
	  /** @} */

	  /**
	   * @brief 深度比較サンプラ
	   *
	   * @details
	   * シャドウマップなどで使用する比較サンプラ。
	   * SampleCmp を利用することを前提とする。
	   *
	   * @warning
	   * 通常の Sample() ではなく SampleCmp() を使用する必要あり。
	   */
#define COMPARISON_DEPTH 13

	   /**
		* @brief サンプラ総数
		*
		* @note
		* Render_State::_sampler_states のサイズと一致させること
		*/
#define SAMPLER_COUNT 14

		/**
		 * @code
		 * // 使用例（C++側）
		 * auto& rs = Render_State::instance();
		 * rs.set_sampler_state(context);
		 *
		 * // シェーダー側（HLSL）
		 * Texture2D tex : register(t0);
		 * SamplerState samp : register(s0); // LINEAR_WRAP を想定
		 *
		 * float4 color = tex.Sample(samp, uv);
		 *
		 * // 深度比較
		 * SamplerComparisonState shadowSamp : register(s1);
		 * float shadow = shadowMap.SampleCmp(shadowSamp, uv, depth);
		 * @endcode
		 */

#endif // __SAMPLERS_H__