#pragma once

/**
 * @file scene_includes.h
 * @brief シーン関連ヘッダ一括インクルード
 *
 * @details
 * 各シーンクラスおよびシーンマネージャをまとめてインクルードする。
 * 主にアプリケーションのエントリポイントやシーン切り替え管理で使用。
 *
 * @note
 * ・インクルードの簡略化が目的
 * ・シーン追加時はここに追記することで一元管理可能
 *
 * @warning
 * ・依存関係が増えやすいためビルド時間に影響する可能性あり
 * ・循環参照が発生しないよう設計に注意
 * ・ヘッダの肥大化を防ぐため、cpp側での使用も検討すること
 */

#include "scene_manager.h"   ///< シーン管理クラス
#include "scene_title.h"     ///< タイトル画面
#include "scene_indoor.h"    ///< ゲーム本編（屋内シーン）
#include "loading.h"         ///< ローディングシーン

 /**
  * @code
  * // 使用例
  * #include "scene_includes.h"
  *
  * Scene_Manager manager;
  * manager.change_scene(new Scene_Title());
  * @endcode
  */