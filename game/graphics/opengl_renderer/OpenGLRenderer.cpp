#include "OpenGLRenderer.h"

#include "common/log/log.h"
#include "game/graphics/pipelines/opengl.h"
#include "game/graphics/opengl_renderer/DirectRenderer.h"
#include "game/graphics/opengl_renderer/SpriteRenderer.h"
#include "game/graphics/opengl_renderer/TextureUploadHandler.h"
#include "third-party/imgui/imgui.h"
#include "common/util/FileUtil.h"
#include "game/graphics/opengl_renderer/SkyRenderer.h"
#include "game/graphics/opengl_renderer/Sprite3.h"
#include "game/graphics/opengl_renderer/background/TFragment.h"
#include "game/graphics/opengl_renderer/background/Tie3.h"
#include "game/graphics/opengl_renderer/MercRenderer.h"
#include "game/graphics/opengl_renderer/EyeRenderer.h"
#include "game/graphics/opengl_renderer/GenericRenderer.h"
#include "game/graphics/opengl_renderer/ocean/OceanMidAndFar.h"
#include "game/graphics/opengl_renderer/ocean/OceanNear.h"

// for the vif callback
#include "game/kernel/kmachine.h"
namespace {
std::string g_current_render;

}

/*!
 * OpenGL Error callback. If we do something invalid, this will be called.
 */
void GLAPIENTRY opengl_error_callback(GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      GLsizei /*length*/,
                                      const GLchar* message,
                                      const void* /*userParam*/) {
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
    lg::debug("OpenGL notification 0x{:X} S{:X} T{:X}: {}", id, source, type, message);
  } else if (severity == GL_DEBUG_SEVERITY_LOW) {
    lg::info("[{}] OpenGL message 0x{:X} S{:X} T{:X}: {}", g_current_render, id, source, type,
             message);
  } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
    lg::warn("[{}] OpenGL warn 0x{:X} S{:X} T{:X}: {}", g_current_render, id, source, type,
             message);
  } else if (severity == GL_DEBUG_SEVERITY_HIGH) {
    lg::error("[{}] OpenGL error 0x{:X} S{:X} T{:X}: {}", g_current_render, id, source, type,
              message);
    // ASSERT(false);
  }
}

OpenGLRenderer::OpenGLRenderer(std::shared_ptr<TexturePool> texture_pool,
                               std::shared_ptr<Loader> loader)
    : m_render_state(texture_pool, loader) {
  // setup OpenGL errors

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(opengl_error_callback, nullptr);
  // disable specific errors
  const GLuint gl_error_ignores_api_other[1] = {0x20071};
  glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1,
                        &gl_error_ignores_api_other[0], GL_FALSE);

  lg::debug("OpenGL context information: {}", (const char*)glGetString(GL_VERSION));

  // initialize all renderers
  init_bucket_renderers();
}

/*!
 * Construct bucket renderers.  We can specify different renderers for different buckets
 */
void OpenGLRenderer::init_bucket_renderers() {
  m_bucket_categories.fill(BucketCategory::OTHER);
  std::vector<tfrag3::TFragmentTreeKind> normal_tfrags = {tfrag3::TFragmentTreeKind::NORMAL,
                                                          tfrag3::TFragmentTreeKind::LOWRES};
  std::vector<tfrag3::TFragmentTreeKind> dirt_tfrags = {tfrag3::TFragmentTreeKind::DIRT};
  std::vector<tfrag3::TFragmentTreeKind> ice_tfrags = {tfrag3::TFragmentTreeKind::ICE};
  auto sky_gpu_blender = std::make_shared<SkyBlendGPU>();
  auto sky_cpu_blender = std::make_shared<SkyBlendCPU>();

  //-------------
  // PRE TEXTURE
  //-------------
  // 0
  // 1
  // 2
  init_bucket_renderer<SkyRenderer>("sky", BucketCategory::OTHER, BucketId::SKY_DRAW);  // 3
  init_bucket_renderer<OceanMidAndFar>("ocean-mid-far", BucketCategory::OCEAN,
                                       BucketId::OCEAN_MID_AND_FAR);  // 4

  //-----------------------
  // LEVEL 0 tfrag texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l0-tfrag-tex", BucketCategory::TEX,
                                             BucketId::TFRAG_TEX_LEVEL0);  // 5
  init_bucket_renderer<TFragment>("l0-tfrag-tfrag", BucketCategory::TFRAG, BucketId::TFRAG_LEVEL0,
                                  normal_tfrags, false,
                                  0);  // 6
  // 7
  // 8
  init_bucket_renderer<Tie3>("l0-tfrag-tie", BucketCategory::TIE, BucketId::TIE_LEVEL0, 0);  // 9
  init_bucket_renderer<MercRenderer>("l0-tfrag-merc", BucketCategory::MERC,
                                     BucketId::MERC_TFRAG_TEX_LEVEL0);  // 10
  init_bucket_renderer<GenericRenderer>("l0-tfrag-gmerc", BucketCategory::GENERIC_MERC,
                                        BucketId::GMERC_TFRAG_TEX_LEVEL0);  // 11

  //-----------------------
  // LEVEL 1 tfrag texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l1-tfrag-tex", BucketCategory::TEX,
                                             BucketId::TFRAG_TEX_LEVEL1);  // 12
  init_bucket_renderer<TFragment>("l1-tfrag-tfrag", BucketCategory::TFRAG, BucketId::TFRAG_LEVEL1,
                                  normal_tfrags, false, 1);
  // 14
  // 15
  init_bucket_renderer<Tie3>("l1-tfrag-tie", BucketCategory::TIE, BucketId::TIE_LEVEL1, 1);
  init_bucket_renderer<MercRenderer>("l1-tfrag-merc", BucketCategory::MERC,
                                     BucketId::MERC_TFRAG_TEX_LEVEL1);  // 17
  init_bucket_renderer<GenericRenderer>("l1-tfrag-gmerc", BucketCategory::GENERIC_MERC,
                                        BucketId::GMERC_TFRAG_TEX_LEVEL1);  // 18

  //-----------------------
  // LEVEL 0 shrub texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l0-shrub-tex", BucketCategory::TEX,
                                             BucketId::SHRUB_TEX_LEVEL0);  // 19
  // 20
  // 21
  // 22
  // 23
  // 24

  //-----------------------
  // LEVEL 1 shrub texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l1-shrub-tex", BucketCategory::TEX,
                                             BucketId::SHRUB_TEX_LEVEL1);  // 25
  // 26
  // 27
  // 28
  // 29

  // I don't think this is actually used? or it might be wrong.
  init_bucket_renderer<GenericRenderer>("common-shrub-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_SHRUB);  // 30

  //-----------------------
  // LEVEL 0 alpha texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l0-alpha-tex", BucketCategory::TEX,
                                             BucketId::ALPHA_TEX_LEVEL0);  // 31
  init_bucket_renderer<SkyBlendHandler>("l0-alpha-sky-blend-and-tfrag-trans", BucketCategory::OTHER,
                                        BucketId::TFRAG_TRANS0_AND_SKY_BLEND_LEVEL0, 0,
                                        sky_gpu_blender, sky_cpu_blender);  // 32
  // 33
  init_bucket_renderer<TFragment>("l0-alpha-tfrag", BucketCategory::TFRAG,
                                  BucketId::TFRAG_DIRT_LEVEL0, dirt_tfrags, false,
                                  0);  // 34
  // 35
  init_bucket_renderer<TFragment>("l0-alpha-tfrag-ice", BucketCategory::TFRAG,
                                  BucketId::TFRAG_ICE_LEVEL0, ice_tfrags, false, 0);
  // 37

  //-----------------------
  // LEVEL 1 alpha texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l1-alpha-tex", BucketCategory::TEX,
                                             BucketId::ALPHA_TEX_LEVEL1);  // 38
  init_bucket_renderer<SkyBlendHandler>("l1-alpha-sky-blend-and-tfrag-trans", BucketCategory::OTHER,
                                        BucketId::TFRAG_TRANS1_AND_SKY_BLEND_LEVEL1, 1,
                                        sky_gpu_blender, sky_cpu_blender);  // 39
  // 40
  init_bucket_renderer<TFragment>("l1-alpha-tfrag-dirt", BucketCategory::TFRAG,
                                  BucketId::TFRAG_DIRT_LEVEL1, dirt_tfrags, false,
                                  1);  // 41
  // 42
  init_bucket_renderer<TFragment>("l1-alpha-tfrag-ice", BucketCategory::TFRAG,
                                  BucketId::TFRAG_ICE_LEVEL1, ice_tfrags, false, 1);
  // 44

  init_bucket_renderer<MercRenderer>("common-alpha-merc", BucketCategory::MERC,
                                     BucketId::MERC_AFTER_ALPHA);

  init_bucket_renderer<GenericRenderer>("common-alpha-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_ALPHA);  // 46
  // 47?

  //-----------------------
  // LEVEL 0 pris texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l0-pris-tex", BucketCategory::TEX,
                                             BucketId::PRIS_TEX_LEVEL0);  // 48
  init_bucket_renderer<MercRenderer>("l0-pris-merc", BucketCategory::MERC,
                                     BucketId::MERC_PRIS_LEVEL0);  // 49
  init_bucket_renderer<GenericRenderer>("l0-pris-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_PRIS_LEVEL0);  // 50

  //-----------------------
  // LEVEL 1 pris texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l1-pris-tex", BucketCategory::TEX,
                                             BucketId::PRIS_TEX_LEVEL1);  // 51
  init_bucket_renderer<MercRenderer>("l1-pris-merc", BucketCategory::MERC,
                                     BucketId::MERC_PRIS_LEVEL1);  // 52
  init_bucket_renderer<GenericRenderer>("l1-pris-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_PRIS_LEVEL1);  // 53

  // other renderers may output to the eye renderer
  m_render_state.eye_renderer = init_bucket_renderer<EyeRenderer>(
      "common-pris-eyes", BucketCategory::OTHER, BucketId::MERC_EYES_AFTER_PRIS);  // 54
  init_bucket_renderer<MercRenderer>("common-pris-merc", BucketCategory::MERC,
                                     BucketId::MERC_AFTER_PRIS);  // 55
  init_bucket_renderer<GenericRenderer>("common-pris-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_PRIS);  // 56

  //-----------------------
  // LEVEL 0 water texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l0-water-tex", BucketCategory::TEX,
                                             BucketId::WATER_TEX_LEVEL0);  // 57
  init_bucket_renderer<MercRenderer>("l0-water-merc", BucketCategory::MERC,
                                     BucketId::MERC_WATER_LEVEL0);  // 58
  init_bucket_renderer<GenericRenderer>("l0-water-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_WATER_LEVEL0);  // 59

  //-----------------------
  // LEVEL 1 water texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("l1-water-tex", BucketCategory::TEX,
                                             BucketId::WATER_TEX_LEVEL1);  // 60
  init_bucket_renderer<MercRenderer>("l1-water-merc", BucketCategory::MERC,
                                     BucketId::MERC_WATER_LEVEL1);  // 61
  init_bucket_renderer<GenericRenderer>("l1-water-generic", BucketCategory::GENERIC_MERC,
                                        BucketId::GENERIC_WATER_LEVEL1);  // 62

  init_bucket_renderer<OceanNear>("ocean-near", BucketCategory::OCEAN, BucketId::OCEAN_NEAR);  // 63
  // 64?

  //-----------------------
  // COMMON texture
  //-----------------------
  init_bucket_renderer<TextureUploadHandler>("common-tex", BucketCategory::TEX,
                                             BucketId::PRE_SPRITE_TEX);  // 65

  std::vector<std::unique_ptr<BucketRenderer>> sprite_renderers;
  // the first renderer added will be the default for sprite.
  sprite_renderers.push_back(std::make_unique<Sprite3>("sprite-3", BucketId::SPRITE));
  sprite_renderers.push_back(std::make_unique<SpriteRenderer>("sprite-renderer", BucketId::SPRITE));
  init_bucket_renderer<RenderMux>("sprite", BucketCategory::SPRITE, BucketId::SPRITE,
                                  std::move(sprite_renderers));  // 66

  init_bucket_renderer<DirectRenderer>("debug-draw-0", BucketCategory::DEBUG_DRAW,
                                       BucketId::DEBUG_DRAW_0, 0x20000);
  init_bucket_renderer<DirectRenderer>("debug-draw-1", BucketCategory::DEBUG_DRAW,
                                       BucketId::DEBUG_DRAW_1, 0x8000);

  // for now, for any unset renderers, just set them to an EmptyBucketRenderer.
  for (size_t i = 0; i < m_bucket_renderers.size(); i++) {
    if (!m_bucket_renderers[i]) {
      init_bucket_renderer<EmptyBucketRenderer>(fmt::format("bucket{}", i), BucketCategory::OTHER,
                                                (BucketId)i);
    }

    m_bucket_renderers[i]->init_shaders(m_render_state.shaders);
    m_bucket_renderers[i]->init_textures(*m_render_state.texture_pool);
  }
  sky_cpu_blender->init_textures(*m_render_state.texture_pool);
  sky_gpu_blender->init_textures(*m_render_state.texture_pool);
  m_render_state.loader->load_common(*m_render_state.texture_pool, "GAME");
}

/*!
 * Main render function. This is called from the gfx loop with the chain passed from the game.
 */
void OpenGLRenderer::render(DmaFollower dma, const RenderOptions& settings) {
  m_profiler.clear();
  m_render_state.reset();
  m_render_state.ee_main_memory = g_ee_main_mem;
  m_render_state.offset_of_s7 = offset_of_s7();
  m_render_state.has_camera_planes = false;

  {
    auto prof = m_profiler.root()->make_scoped_child("frame-setup");
    setup_frame(settings.window_width_px, settings.window_height_px, settings.lbox_width_px,
                settings.lbox_height_px);
  }

  // draw_test_triangle();
  // render the buckets!
  {
    auto prof = m_profiler.root()->make_scoped_child("buckets");
    dispatch_buckets(dma, prof);
  }

  if (settings.draw_render_debug_window) {
    auto prof = m_profiler.root()->make_scoped_child("render-window");
    draw_renderer_selection_window();
    // add a profile bar for the imgui stuff
    vif_interrupt_callback();
  }

  {
    auto prof = m_profiler.root()->make_scoped_child("loader");
    m_render_state.loader->update(m_render_state.load_status_debug, *m_render_state.texture_pool);
  }

  m_profiler.finish();
  if (settings.draw_profiler_window) {
    m_profiler.draw();
  }

  //  if (m_profiler.root_time() > 0.018) {
  //    fmt::print("Slow frame: {:.2f} ms\n", m_profiler.root_time() * 1000);
  //    fmt::print("{}\n", m_profiler.to_string());
  //  }

  if (settings.draw_small_profiler_window) {
    SmallProfilerStats stats;
    stats.draw_calls = m_profiler.root()->stats().draw_calls;
    stats.triangles = m_profiler.root()->stats().triangles;
    for (int i = 0; i < (int)BucketCategory::MAX_CATEGORIES; i++) {
      stats.time_per_category[i] = m_category_times[i];
    }
    m_small_profiler.draw(m_render_state.load_status_debug, stats);
  }

  if (settings.save_screenshot) {
    finish_screenshot(settings.screenshot_path, settings.window_width_px, settings.window_height_px,
                      settings.lbox_width_px, settings.lbox_height_px);
  }
}

/*!
 * Draw the per-renderer debug window
 */
void OpenGLRenderer::draw_renderer_selection_window() {
  ImGui::Begin("Renderer Debug");

  ImGui::SliderFloat("Fog Adjust", &m_render_state.fog_intensity, 0, 10);
  ImGui::Checkbox("Sky CPU", &m_render_state.use_sky_cpu);
  ImGui::Checkbox("Occlusion Cull", &m_render_state.use_occlusion_culling);
  ImGui::Checkbox("Render Debug (slower)", &m_render_state.render_debug);
  ImGui::Checkbox("Merc XGKICK", &m_render_state.enable_merc_xgkick);
  ImGui::Checkbox("Generic XGKICK", &m_render_state.enable_generic_xgkick);
  ImGui::Checkbox("Direct 2", &m_render_state.use_direct2);
  ImGui::Checkbox("Generic 2", &m_render_state.use_generic2);

  for (size_t i = 0; i < m_bucket_renderers.size(); i++) {
    auto renderer = m_bucket_renderers[i].get();
    if (renderer && !renderer->empty()) {
      ImGui::PushID(i);
      if (ImGui::TreeNode(renderer->name_and_id().c_str())) {
        ImGui::Checkbox("Enable", &renderer->enabled());
        renderer->draw_debug_window();
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }
  if (ImGui::TreeNode("Texture Pool")) {
    m_render_state.texture_pool->draw_debug_window();
    ImGui::TreePop();
  }
  ImGui::End();
}

/*!
 * Pre-render frame setup.
 */
void OpenGLRenderer::setup_frame(int window_width_px,
                                 int window_height_px,
                                 int offset_x,
                                 int offset_y) {
  glViewport(offset_x, offset_y, window_width_px, window_height_px);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClearDepth(0.0);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_BLEND);
}

/*!
 * This function finds buckets and dispatches them to the appropriate part.
 */
void OpenGLRenderer::dispatch_buckets(DmaFollower dma, ScopedProfilerNode& prof) {
  // The first thing the DMA chain should be a call to a common default-registers chain.
  // this chain resets the state of the GS. After this is buckets
  m_category_times.fill(0);

  m_render_state.buckets_base =
      dma.current_tag_offset() + 16;  // offset by 1 qw for the initial call
  m_render_state.next_bucket = m_render_state.buckets_base;

  // Find the default regs buffer
  auto initial_call_tag = dma.current_tag();
  ASSERT(initial_call_tag.kind == DmaTag::Kind::CALL);
  auto initial_call_default_regs = dma.read_and_advance();
  ASSERT(initial_call_default_regs.transferred_tag == 0);  // should be a nop.
  m_render_state.default_regs_buffer = dma.current_tag_offset();
  auto default_regs_tag = dma.current_tag();
  ASSERT(default_regs_tag.kind == DmaTag::Kind::CNT);
  ASSERT(default_regs_tag.qwc == 10);
  // TODO verify data in here.
  auto default_data = dma.read_and_advance();
  ASSERT(default_data.size_bytes > 148);
  memcpy(m_render_state.fog_color.data(), default_data.data + 144, 4);
  auto default_ret_tag = dma.current_tag();
  ASSERT(default_ret_tag.qwc == 0);
  ASSERT(default_ret_tag.kind == DmaTag::Kind::RET);
  dma.read_and_advance();

  // now we should point to the first bucket!
  ASSERT(dma.current_tag_offset() == m_render_state.next_bucket);
  m_render_state.next_bucket += 16;

  // loop over the buckets!
  for (int bucket_id = 0; bucket_id < (int)BucketId::MAX_BUCKETS; bucket_id++) {
    auto& renderer = m_bucket_renderers[bucket_id];
    auto bucket_prof = prof.make_scoped_child(renderer->name_and_id());
    g_current_render = renderer->name_and_id();
    // lg::info("Render: {} start", g_current_render);
    renderer->render(dma, &m_render_state, bucket_prof);
    // lg::info("Render: {} end", g_current_render);
    //  should have ended at the start of the next chain
    ASSERT(dma.current_tag_offset() == m_render_state.next_bucket);
    m_render_state.next_bucket += 16;
    vif_interrupt_callback();
    m_category_times[(int)m_bucket_categories[bucket_id]] += bucket_prof.get_elapsed_time();
  }
  g_current_render = "";

  // TODO ending data.
}

void OpenGLRenderer::draw_test_triangle() {
  // just remembering how to use opengl here.

  //////////
  // Setup
  //////////

  // create "buffer object names"
  GLuint vertex_buffer, color_buffer, vao;
  glGenBuffers(1, &vertex_buffer);
  glGenBuffers(1, &color_buffer);

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // set vertex data
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  const float verts[9] = {0.0, 0.8, 0, -0.5, -0.5 * .866, 0, 0.5, -0.5 * .866, 0};
  glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), verts, GL_STATIC_DRAW);

  // set color data
  glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
  const float colors[12] = {1., 0, 0., 1., 0., 1., 0., 1., 0., 0., 1., 1.};
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), colors, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  //////////
  // Draw!
  //////////
  m_render_state.shaders[ShaderId::TEST_SHADER].activate();

  // location 0: the vertices
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,         // location 0 in the shader
                        3,         // 3 floats per vert
                        GL_FLOAT,  // floats
                        GL_FALSE,  // normalized, ignored,
                        0,         // tightly packed
                        0          // offset in array (why is is this a pointer...)
  );

  glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);

  ////////////
  // Clean Up
  ////////////
  // delete buffer
  glDeleteBuffers(1, &color_buffer);
  glDeleteBuffers(1, &vertex_buffer);
  glDeleteVertexArrays(1, &vao);
}

/*!
 * Take a screenshot!
 */
void OpenGLRenderer::finish_screenshot(const std::string& output_name,
                                       int width,
                                       int height,
                                       int x,
                                       int y) {
  std::vector<u32> buffer(width * height);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
  // flip upside down in place
  for (int h = 0; h < height / 2; h++) {
    for (int w = 0; w < width; w++) {
      std::swap(buffer[h * width + w], buffer[(height - h - 1) * width + w]);
    }
  }

  // set alpha. For some reason, image viewers do weird stuff with alpha.
  for (auto& px : buffer) {
    px |= 0xff000000;
  }
  file_util::write_rgba_png(output_name, buffer.data(), width, height);
}
