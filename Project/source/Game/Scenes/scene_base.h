#pragma once
#include <stdio.h>
#include "Engine/utilities/misc.h"

/**
 * @brief シーン基底クラス
 *
 * @details
 * すべてのシーン（タイトル・ゲーム・ローディングなど）の
 * 共通インターフェースを定義する抽象クラス。
 *
 * シーンのライフサイクル：
 * 1. initialize() : 初期化
 * 2. update()     : 更新（毎フレーム）
 * 3. render()     : 描画（毎フレーム）
 * 4. draw_gui()   : GUI描画
 * 5. finalize()   : 終了処理
 *
 * @note
 * ・ポリモーフィズムにより Scene_Manager から統一的に扱える
 * ・_ready フラグにより非同期初期化（ローディング）と連携可能
 *
 * @warning
 * ・デストラクタは必ず virtual（継承前提）
 * ・initialize() を呼ばずに update/render を呼ぶのは禁止
 * ・非同期ロード時は _ready の状態管理に注意
 */
class Scene_Base
{
public:

    /**
     * @brief コンストラクタ
     */
    Scene_Base() {}

    /**
     * @brief 仮想デストラクタ
     */
    virtual ~Scene_Base() {}

    /**
     * @brief 初期化処理
     *
     * @details
     * リソース生成や初期設定を行う。
     *
     * @note
     * 重い処理はローディングスレッドへ分離することを推奨
     */
    virtual void initialize() = 0;

    /**
     * @brief 終了処理
     *
     * @details
     * リソース解放などを行う。
     *
     * @warning
     * GPUリソースは必ずここで解放すること
     */
    virtual void finalize() = 0;

    /**
     * @brief 更新処理
     * @param elapsedTime 経過時間（秒）
     *
     * @details
     * ゲームロジック・入力処理などを行う。
     */
    virtual void update(float elapsedTime) = 0;

    /**
     * @brief 描画処理
     * @param elapsedTime 経過時間（秒）
     *
     * @details
     * シーンの描画処理を行う。
     *
     * @note
     * Direct3D の描画は基本的にこの関数で行う
     */
    virtual void render(float elapsedTime) = 0;

    /**
     * @brief GUI描画
     *
     * @details
     * ImGuiなどのデバッグUI描画用
     */
    virtual void draw_gui() = 0;

    /**
     * @brief 準備完了状態の取得
     * @return true 準備完了
     * @return false 未完了
     *
     * @note
     * 主にローディングシーンとの連携に使用
     */
    bool is_ready() const { return _ready; }

    /**
     * @brief 準備完了フラグ設定
     *
     * @warning
     * 非同期スレッドから呼ぶ場合は競合に注意（atomic推奨）
     */
    void set_ready() { _ready = true; }

private:

    /**
     * @brief 初期化完了フラグ
     *
     * @note
     * ローディングシーンなどで利用
     */
    bool _ready = false;
};