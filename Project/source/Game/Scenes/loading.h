#pragma once

#include "scene_base.h"
#include "Engine/Graphics/UI/sprite/sprite.h"
#include "Engine/system/graphics_core.h"

#include "Engine/Graphics/UI/text/text.h"
#include "Engine/System/Manager/resource_manager.h"
#include <thread>

/**
 * @brief ローディングシーン
 *
 * @details
 * 次のシーンへ遷移するためのリソースロードをバックグラウンドスレッドで行い、
 * ローディング画面を表示するシーン。
 *
 * 処理フロー：
 * 1. initialize() でローディングスレッド開始
 * 2. 別スレッドでリソース読み込み
 * 3. update() で完了判定
 * 4. 完了後に次シーンへ遷移
 *
 * @note
 * ・非同期ロードによりフレーム落ちを防ぐ設計
 * ・描画はメインスレッドのみで実行する（D3D制約）
 * ・Scene_Base を継承したシンプルな遷移管理
 *
 * @warning
 * ・D3Dリソース生成はスレッドセーフではないため注意
 * ・スレッド終了前にオブジェクトが破棄されないようにすること
 * ・_thread は必ず join() または detach() する必要がある
 */
class Scene_Loading : public Scene_Base
{
public:

    /**
     * @brief コンストラクタ
     * @param next_scene 遷移先シーン
     *
     * @warning
     * next_scene は有効なポインタである必要がある（nullptr不可）
     */
    Scene_Loading(Scene_Base* next_scene)
        : _next_scene(next_scene) {
    }

    ~Scene_Loading() {}

    /**
     * @brief 初期化処理
     *
     * @details
     * ・ローディングスレッドを起動
     * ・ロゴスプライトなどの表示準備
     */
    void initialize() override;

    /**
     * @brief 終了処理
     *
     * @note
     * スレッドの終了を保証することが重要
     */
    void finalize() override;

    /**
     * @brief 更新処理
     * @param elapsedTime 経過時間
     *
     * @details
     * ロード完了を監視し、完了後にシーン遷移を行う
     */
    void update(float elapsedTime) override;

    /**
     * @brief 描画処理
     * @param elapsedTime 経過時間
     *
     * @note
     * ローディング画面（ロゴ・アニメーションなど）を描画
     */
    void render(float elapsedTime) override;

    /**
     * @brief GUI描画
     */
    void draw_gui() override;

private:

    /**
     * @brief ローディングスレッド処理
     * @param scene このインスタンス
     *
     * @details
     * リソースの読み込み処理を実行する。
     *
     * @code
     * // 例
     * ResourceManager::instance().load_all();
     * @endcode
     *
     * @warning
     * ・Direct3Dのデバイスコンテキスト操作は禁止（メインスレッド限定）
     * ・Scene_Loading のメンバへアクセスする場合は同期に注意
     */
    static void loading_thread(Scene_Loading* scene);

private:

    ///< 遷移先シーン
    Scene_Base* _next_scene = nullptr;

    ///< ローディング用スレッド
    std::thread* _thread = nullptr;

    ///< ロゴスプライト
    sprite* _logo_sprite = nullptr;

    ///< 経過時間（アニメーション用）
    float _timer = 0.0f;
};