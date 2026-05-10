#include "gui_engine.h"
#include "utils/pattern.h"
#include "utils/estimate.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <mutex>
#include <fstream>

using namespace trx;

// ============================================================================
// GUI Application State
// ============================================================================

static std::unique_ptr<GUIEngine> g_engine;
static std::atomic<bool> g_shutting_down{false};

// Widgets
static Fl_Window*      g_main_win = nullptr;
static Fl_Choice*      g_pattern_type = nullptr;
static Fl_Input*       g_pattern_arg = nullptr;
static Fl_Input*       g_pattern_arg2 = nullptr;
static Fl_Spinner*     g_threads = nullptr;
static Fl_Check_Button* g_use_gpu = nullptr;
static Fl_Button*      g_btn_start = nullptr;
static Fl_Button*      g_btn_pause = nullptr;
static Fl_Button*      g_btn_stop = nullptr;
static Fl_Button*      g_btn_export = nullptr;
static Fl_Text_Display* g_log_display = nullptr;
static Fl_Text_Buffer* g_log_buffer = nullptr;
static Fl_Box*         g_stats_box = nullptr;
static Fl_Scroll*      g_results_scroll = nullptr;

// Result rows (dynamic)
struct ResultRow {
    Fl_Box* addr_label = nullptr;
    Fl_Button* reveal_btn = nullptr;
    Fl_Box* key_label = nullptr;
    GUIMatchResult data;
};
static std::vector<ResultRow> g_result_rows;
static std::mutex g_results_mutex;

// ============================================================================
// Helpers
// ============================================================================

static void append_log(const char* msg) {
    if (!g_log_buffer) return;
    g_log_buffer->append(msg);
    g_log_buffer->append("\n");
    if (g_log_display) {
        g_log_display->scroll(g_log_buffer->length(), 0);
    }
}

static std::string format_stats(const GUIStats& s) {
    std::ostringstream oss;
    oss << "尝试: " << s.total_attempts
        << " | 命中: " << s.match_count
        << " | 速率: " << std::fixed << std::setprecision(1) << s.rate_per_second << " addr/s"
        << " | 用时: " << std::setprecision(1) << s.elapsed_seconds << "s"
        << " | 状态: " << (s.is_running ? (s.is_paused ? "暂停" : "运行中") : "停止");
    return oss.str();
}

static void update_stats_cb(const GUIStats& s) {
    if (!g_stats_box) return;
    std::string text = format_stats(s);
    g_stats_box->label(text.c_str());
    g_stats_box->redraw();
}

static void on_reveal_key(Fl_Widget* w, void* userdata) {
    ResultRow* row = static_cast<ResultRow*>(userdata);
    if (!row) return;

    // Secondary confirmation dialog
    int ok = fl_choice("确认显示私钥？", "取消", "确认显示", nullptr);
    if (ok != 1) return;

    // Show the private key in the label (use copy_label so string persists)
    if (row->key_label) {
        std::string label = "私钥: " + row->data.private_key_hex;
        row->key_label->copy_label(label.c_str());
        row->key_label->redraw();
    }
    if (row->reveal_btn) {
        row->reveal_btn->deactivate();
        row->reveal_btn->label("已显示");
    }
}

static void add_result_row(const GUIMatchResult& match) {
    std::lock_guard<std::mutex> lock(g_results_mutex);
    if (!g_results_scroll) return;

    int y = 10 + static_cast<int>(g_result_rows.size()) * 60;
    int w = g_results_scroll->w() - 30;

    // FLTK does NOT copy label strings — we must store them persistently.
    std::string addr_label_text = "地址: " + match.address;
    Fl_Box* addr = new Fl_Box(10, y, w, 20, "");
    addr->copy_label(addr_label_text.c_str());
    addr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    addr->box(FL_NO_BOX);

    Fl_Button* reveal = new Fl_Button(10, y + 22, 120, 24, "显示私钥");
    Fl_Box* key = new Fl_Box(140, y + 22, w - 140, 24, "私钥: ******** (点击显示)");
    key->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    key->box(FL_NO_BOX);

    ResultRow row;
    row.addr_label = addr;
    row.reveal_btn = reveal;
    row.key_label = key;
    row.data = match; // copy full data including hidden key
    g_result_rows.push_back(row);

    reveal->callback(on_reveal_key, &g_result_rows.back());

    g_results_scroll->add(addr);
    g_results_scroll->add(reveal);
    g_results_scroll->add(key);
    g_results_scroll->redraw();
}

static void on_match_cb(const GUIMatchResult& match) {
    if (g_shutting_down.load()) return;
    // Log match (without private key)
    std::string msg = "命中: " + match.address + " [" + match.pattern_matched + "]";
    append_log(msg.c_str());
    // Add UI row
    Fl::lock();
    add_result_row(match);
    Fl::unlock();
    Fl::awake();
}

// ============================================================================
// Engine callbacks
// ============================================================================

static void start_generation() {
    if (g_engine && g_engine->is_running()) {
        append_log("已经在运行中");
        return;
    }

    const char* type_text = g_pattern_type->text();
    if (!type_text) {
        fl_alert("请选择规则类型");
        return;
    }
    std::string pattern_type(type_text);
    std::string arg1(g_pattern_arg->value() ? g_pattern_arg->value() : "");
    std::string arg2(g_pattern_arg2->value() ? g_pattern_arg2->value() : "");

    if (arg1.empty()) {
        fl_alert("请输入规则参数");
        return;
    }

    PatternType pt;
    if (pattern_type == "suffix") pt = PatternType::SUFFIX_CUSTOM;
    else if (pattern_type == "prefix") pt = PatternType::PREFIX_CUSTOM;
    else if (pattern_type == "contains") pt = PatternType::CONTAINS;
    else if (pattern_type == "consecutive") pt = PatternType::SUFFIX_CONSECUTIVE;
    else if (pattern_type == "sequential") pt = PatternType::SUFFIX_SEQUENTIAL;
    else {
        fl_alert("未知的规则类型");
        return;
    }

    std::unique_ptr<Pattern> pattern;
    try {
        pattern = PatternFactory::create(pt, arg1, arg2);
    } catch (const std::exception& e) {
        fl_alert("规则创建失败: %s", e.what());
        return;
    }

    g_engine = std::make_unique<GUIEngine>();
    g_engine->set_pattern(std::move(pattern));
    g_engine->set_num_threads(static_cast<size_t>(g_threads->value()));
    if (g_use_gpu->value()) {
        g_engine->set_mode(GUIMode::GPU);
    } else {
        g_engine->set_mode(GUIMode::CPU);
    }

    g_engine->set_match_callback(on_match_cb);
    g_engine->set_stats_callback(update_stats_cb);

    // Clear previous results
    {
        std::lock_guard<std::mutex> lock(g_results_mutex);
        g_result_rows.clear();
        if (g_results_scroll) {
            g_results_scroll->clear();
            g_results_scroll->redraw();
        }
    }
    if (g_log_buffer) g_log_buffer->text("");

    g_engine->start();
    append_log("开始生成...");

    g_btn_start->deactivate();
    g_btn_pause->activate();
    g_btn_stop->activate();
    g_btn_export->deactivate();
}

static void pause_generation() {
    if (!g_engine) return;
    if (g_engine->is_paused()) {
        g_engine->resume();
        g_btn_pause->label("暂停");
        append_log("已恢复");
    } else {
        g_engine->pause();
        g_btn_pause->label("恢复");
        append_log("已暂停");
    }
}

static void stop_generation() {
    if (!g_engine) return;
    append_log("正在停止...");
    g_engine->stop();
    g_engine.reset();
    append_log("已停止");

    g_btn_start->activate();
    g_btn_pause->deactivate();
    g_btn_stop->deactivate();
    g_btn_export->activate();
    g_btn_pause->label("暂停");
}

// ============================================================================
// Export dialog
// ============================================================================

static void show_export_dialog() {
    if (!g_engine) {
        fl_alert("没有运行中的引擎，无法导出");
        return;
    }

    auto matches = g_engine->get_matches();
    if (matches.empty()) {
        fl_alert("没有可导出的结果");
        return;
    }

    // Ask for format
    int fmt_choice = fl_choice("选择导出格式", "取消", "CSV", "JSON");
    if (fmt_choice == 0) return; // cancelled
    GUIEngine::ExportFormat format = (fmt_choice == 1)
        ? GUIEngine::ExportFormat::CSV
        : GUIEngine::ExportFormat::JSON;

    // File chooser
    Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.title("保存导出文件");
    if (format == GUIEngine::ExportFormat::CSV) {
        chooser.filter("CSV Files\t*.csv\nAll Files\t*.*");
        chooser.preset_file("trx_vanity_results.csv");
    } else {
        chooser.filter("JSON Files\t*.json\nAll Files\t*.*");
        chooser.preset_file("trx_vanity_results.json");
    }

    if (chooser.show() != 0) {
        append_log("导出已取消");
        return;
    }
    std::string filepath = chooser.filename();

    // Password dialog
    Fl_Window* pwd_win = new Fl_Window(360, 140, "设置导出密码");
    Fl_Secret_Input* pwd_input = new Fl_Secret_Input(20, 40, 320, 30, "密码:");
    pwd_input->tooltip("此密码用于加密私钥，丢失后无法恢复");
    Fl_Secret_Input* pwd_confirm = new Fl_Secret_Input(20, 80, 320, 30, "确认密码:");
    Fl_Button* ok_btn = new Fl_Button(200, 105, 60, 26, "确定");
    Fl_Button* cancel_btn = new Fl_Button(270, 105, 60, 26, "取消");

    bool confirmed = false;
    ok_btn->callback([](Fl_Widget*, void* data) {
        *static_cast<bool*>(data) = true;
        Fl::first_window()->hide();
    }, &confirmed);
    cancel_btn->callback([](Fl_Widget*, void*) {
        Fl::first_window()->hide();
    }, nullptr);

    pwd_win->set_modal();
    pwd_win->end();
    pwd_win->show();
    while (pwd_win->shown()) {
        Fl::wait();
    }

    if (!confirmed) {
        delete pwd_win;
        append_log("导出已取消");
        return;
    }

    std::string pwd = pwd_input->value() ? pwd_input->value() : "";
    std::string pwd2 = pwd_confirm->value() ? pwd_confirm->value() : "";
    delete pwd_win;

    if (pwd.empty()) {
        fl_alert("密码不能为空");
        return;
    }
    if (pwd != pwd2) {
        fl_alert("两次输入的密码不一致");
        return;
    }

    // Security warning
    int warn_ok = fl_choice(
        "安全提示:\n"
        "私钥将使用 AES-256-GCM 加密保存。\n"
        "请务必牢记密码，密码丢失后私钥将无法恢复！\n"
        "导出文件不会自动上传到任何服务。\n\n"
        "确认继续导出？",
        "取消", "确认导出", nullptr);
    if (warn_ok != 1) {
        append_log("导出已取消");
        return;
    }

    // Perform export
    std::string error;
    bool ok = g_engine->export_matches(filepath, format, pwd, error);
    if (!ok) {
        fl_alert("导出失败: %s", error.c_str());
        append_log(("导出失败: " + error).c_str());
        return;
    }

    append_log(("导出成功: " + filepath).c_str());
    fl_message("导出成功!\n文件: %s\n\n请妥善保管密码，密码丢失后私钥将无法恢复。",
               filepath.c_str());
}

// ============================================================================
// Window close handler
// ============================================================================

static void on_window_close(Fl_Widget*, void*) {
    g_shutting_down = true;
    if (g_engine) {
        g_engine->stop();
        g_engine.reset();
    }
    // No private key logging
    exit(0);
}

// ============================================================================
// Build UI
// ============================================================================

int main(int argc, char** argv) {
    Fl::scheme("gtk+");
    Fl::lock(); // enable thread-safe UI updates

    int W = 720, H = 580;
    g_main_win = new Fl_Window(W, H, "TRX Vanity Generator — GUI MVP");
    g_main_win->callback(on_window_close, nullptr);

    // Pattern type
    g_pattern_type = new Fl_Choice(120, 10, 180, 25, "规则类型:");
    g_pattern_type->add("suffix");
    g_pattern_type->add("prefix");
    g_pattern_type->add("contains");
    g_pattern_type->add("consecutive");
    g_pattern_type->add("sequential");
    g_pattern_type->value(0);

    // Pattern arg 1
    g_pattern_arg = new Fl_Input(120, 40, 180, 25, "参数1:");
    g_pattern_arg->tooltip("例如: 8888, ABC, 8");

    // Pattern arg 2
    g_pattern_arg2 = new Fl_Input(120, 70, 180, 25, "参数2:");
    g_pattern_arg2->tooltip("仅 consecutive/sequential 需要长度, 如 7");

    // Threads
    g_threads = new Fl_Spinner(420, 10, 80, 25, "线程数:");
    g_threads->minimum(1);
    g_threads->maximum(64);
    g_threads->value(4);

    // GPU checkbox
    g_use_gpu = new Fl_Check_Button(420, 40, 120, 25, "使用 GPU");

    // Buttons
    g_btn_start = new Fl_Button(520, 10, 80, 25, "开始");
    g_btn_start->callback([](Fl_Widget*, void*) { start_generation(); });

    g_btn_pause = new Fl_Button(520, 40, 80, 25, "暂停");
    g_btn_pause->callback([](Fl_Widget*, void*) { pause_generation(); });
    g_btn_pause->deactivate();

    g_btn_stop = new Fl_Button(610, 10, 80, 25, "停止");
    g_btn_stop->callback([](Fl_Widget*, void*) { stop_generation(); });
    g_btn_stop->deactivate();

    g_btn_export = new Fl_Button(610, 40, 80, 25, "导出结果");
    g_btn_export->callback([](Fl_Widget*, void*) { show_export_dialog(); });
    g_btn_export->deactivate();

    // Stats
    g_stats_box = new Fl_Box(10, 105, W - 20, 20, "准备就绪");
    g_stats_box->box(FL_THIN_DOWN_BOX);
    g_stats_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // Log display
    g_log_buffer = new Fl_Text_Buffer();
    g_log_display = new Fl_Text_Display(10, 135, W - 20, 180);
    g_log_display->buffer(g_log_buffer);
    g_log_display->textfont(FL_COURIER);
    g_log_display->textsize(12);

    // Results scroll area
    g_results_scroll = new Fl_Scroll(10, 325, W - 20, H - 335);
    g_results_scroll->box(FL_THIN_DOWN_BOX);

    g_main_win->end();
    g_main_win->show(argc, argv);

    append_log("TRX Vanity GUI MVP 已启动");
    append_log("安全提示: 私钥默认隐藏，点击“显示私钥”需二次确认");
    append_log("导出结果: 点击“导出结果”可将命中地址加密保存为 CSV/JSON");
    append_log("本窗口关闭时会安全停止后台任务");

    return Fl::run();
}
