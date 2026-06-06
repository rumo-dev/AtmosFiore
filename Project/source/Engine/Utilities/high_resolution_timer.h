#pragma once

#include <windows.h>

/**
 * @class high_resolution_timer
 * @brief 高精度タイマーを提供するクラス
 *
 * WindowsのQueryPerformanceCounterを利用して、フレーム間の経過時間や
 * 累積時間を高精度で計測します。ゲームループやアニメーションの制御など、
 * 精密な時間管理が必要な場面で使用します。
 */
class high_resolution_timer
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * タイマーの初期化を行い、基準時間を設定します。
	 */
	high_resolution_timer()
	{
		LONGLONG counts_per_sec;
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counts_per_sec));
		seconds_per_count = 1.0 / static_cast<double>(counts_per_sec);

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;
	}

	/**
	 * @brief デストラクタ
	 */
	~high_resolution_timer() = default;

	/**
	 * @brief コピーコンストラクタ（削除）
	 */
	high_resolution_timer(const high_resolution_timer&) = delete;

	/**
	 * @brief コピー代入演算子（削除）
	 */
	high_resolution_timer& operator=(const high_resolution_timer&) = delete;

	/**
	 * @brief ムーブコンストラクタ（削除）
	 */
	high_resolution_timer(high_resolution_timer&&) noexcept = delete;

	/**
	 * @brief ムーブ代入演算子（削除）
	 */
	high_resolution_timer& operator=(high_resolution_timer&&) noexcept = delete;

	/**
	 * @brief リセット後からの経過時間（秒）を取得
	 * @return 経過時間（秒）
	 *
	 * Reset()が呼ばれてからの累積時間を返します。
	 * 停止中の時間は含みません。
	 */
	float time_stamp() const
	{
		if (stopped)
		{
			return static_cast<float>(((stop_time - paused_time) - base_time) * seconds_per_count);
		}
		else
		{
			return static_cast<float>(((this_time - paused_time) - base_time) * seconds_per_count);
		}
	}

	/**
	 * @brief 前回tick()からの経過時間（秒）を取得
	 * @return 経過時間（秒）
	 */
	float time_interval() const
	{
		return static_cast<float>(delta_time);
	}

	/**
	 * @brief タイマーをリセットする
	 *
	 * メッセージループの前など、タイマーの基準時間を再設定したい場合に呼び出します。
	 */
	void reset()
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;

		stop_time = 0;
		stopped = false;
	}

	/**
	 * @brief タイマーを再開する
	 *
	 * 一時停止後に再開する場合に呼び出します。
	 */
	void start()
	{
		LONGLONG start_time;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_time));

		if (stopped)
		{
			paused_time += (start_time - stop_time);
			last_time = start_time;
			stop_time = 0;
			stopped = false;
		}
	}

	/**
	 * @brief タイマーを一時停止する
	 *
	 * 一時停止したい場合に呼び出します。
	 */
	void stop()
	{
		if (!stopped)
		{
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&stop_time));
			stopped = true;
		}
	}

	/**
	 * @brief フレーム毎に呼び出し、経過時間を更新する
	 *
	 * 毎フレーム呼び出すことで、前回tick()からの経過時間を計算します。
	 */
	void tick()
	{
		if (stopped)
		{
			delta_time = 0.0;
			return;
		}

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		delta_time = (this_time - last_time) * seconds_per_count;
		last_time = this_time;

		if (delta_time < 0.0)
		{
			delta_time = 0.0;
		}
	}

private:
	/**
	 * @brief 1カウントあたりの秒数
	 */
	double seconds_per_count{ 0.0 };

	/**
	 * @brief 前回tick()からの経過時間（秒）
	 */
	double delta_time{ 0.0 };

	/**
	 * @brief タイマーの基準時間
	 */
	LONGLONG base_time{ 0LL };

	/**
	 * @brief 一時停止中の累積時間
	 */
	LONGLONG paused_time{ 0LL };

	/**
	 * @brief 一時停止した時刻
	 */
	LONGLONG stop_time{ 0LL };

	/**
	 * @brief 前回tick()の時刻
	 */
	LONGLONG last_time{ 0LL };

	/**
	 * @brief 現在時刻
	 */
	LONGLONG this_time{ 0LL };

	/**
	 * @brief タイマーが停止中かどうか
	 */
	bool stopped{ false };
};