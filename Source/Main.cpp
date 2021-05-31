#include "Application.h"
#include "FileLoader.h"
#include "Logger.h"
#include "FirsrPersonCamera.h"
#include "ThirdPersonCamera.h"
#include "Shader.h"
#include "MatrixStack.h"
#include "IsoSurface.h"
#include "Cube.h"
#include "Light.h"
#include "NDCQuad.h"
#include "Sphere.h"

#include <stb_image.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_bazier.h>
#include <implot.h>
#include <algorithm>
#include <random>
#include <transfer_function_widget.h>
#include <transfer_function.h>

class VolumeRendering final : public Nexus::Application {
public:
	VolumeRendering() {
		Settings.Width = 800;
		Settings.Height = 600;
		Settings.WindowTitle = "Scientific Visualization | Project #3: Volume Rendering Using GPU";
		Settings.EnableDebugCallback = true;
		Settings.EnableFullScreen = false;

		Settings.EnableGhostMode = false;
		Settings.ShowOriginAnd3Axes = false;
		Settings.EnableFaceCulling = true;
		Settings.CullingTypeStr = "Front Face";

		// Projection Settings Initalize
		ProjectionSettings.IsPerspective = true;
		ProjectionSettings.ClippingNear = 0.1f;
		ProjectionSettings.ClippingFar = 1000.0f;
		ProjectionSettings.Aspect = static_cast<float>(Settings.Width) / static_cast<float>(Settings.Height);
	}

	void Initialize() override {
		// Setting OpenGL
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// ImGui Setting
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontFromFileTTF("Resource/Fronts/ComicNeue-Regular.ttf", 18.0f);

		// Create shader program
		myShader = std::make_unique<Nexus::Shader>("Shaders/simple_lighting.vert", "Shaders/simple_lighting.frag");
		rayShader = std::make_unique<Nexus::Shader>("Shaders/ray_casting.vert", "Shaders/ray_casting.frag");
		normalShader = std::make_unique<Nexus::Shader>("Shaders/normal_visualization.vs", "Shaders/normal_visualization.fs", "Shaders/normal_visualization.gs");
		screenShader = std::make_unique<Nexus::Shader>("Shaders/framebuffer_screen.vert", "Shaders/framebuffer_screen.frag");

		// Create Camera
		first_camera = std::make_unique<Nexus::FirstPersonCamera>(glm::vec3(0.0f, 0.0f, 430.0f));
		third_camera = std::make_unique<Nexus::ThirdPersonCamera>(glm::vec3(0.0f, 0.0f, 430.0f));

		// Create Matrix Stack
		model = std::make_unique<Nexus::MatrixStack>();

		// Create object data
		engine = std::make_unique<Nexus::IsoSurface>();
		
		cube = std::make_unique<Nexus::Cube>();
		quad = std::make_unique<Nexus::NDCQuad>();
		sphere = std::make_unique<Nexus::Sphere>();

		// Create a transfunction (1D Texture)
		transfer_function_texture = GetTFTexture(tf_widget);

		// Create a point light
		point_light = std::make_unique<Nexus::PointLight>(glm::vec3(0.0f, 0.0f, 0.0f), true);

		// Create a texture color buffer (texture 2D)
		texture_color_buffer = std::make_unique<Nexus::Texture2D>(Settings.Width, Settings.Height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
		texture_color_buffer->SetFilterParams(GL_LINEAR, GL_LINEAR);
		texture_color_buffer->SetWrappingParams(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		// Create Framebuffer and bind the texture
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_color_buffer->ID, 0);

		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Settings.Width, Settings.Height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			// error
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		// glDeleteFramebuffers(1, &framebuffer);
	}

	void Update(Nexus::DisplayMode monitor_type) override {

		SetViewMatrix(Nexus::DISPLAY_MODE_DEFAULT);
		SetProjectionMatrix(Nexus::DISPLAY_MODE_DEFAULT);
		SetViewport(Nexus::DISPLAY_MODE_DEFAULT);

		if (Settings.EnableFaceCulling) {
			// CW => Clockwise 順時針方向為正向面 
			glEnable(GL_CULL_FACE);
			glFrontFace(GL_CW);
			if (Settings.CullingTypeStr == "Back Face") {
				glCullFace(GL_BACK);
			} else {
				glCullFace(GL_FRONT);
			}
		} else {
			glDisable(GL_CULL_FACE);
		}

		// Lighting Setting
		point_light->SetPosition(Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());

		// Shader Init
		myShader->Use();
		myShader->SetMat4("view", view);
		myShader->SetMat4("projection", projection);
		myShader->SetVec3("viewPos", Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());
		myShader->SetVec3("lightPos", point_light->GetPosition());
		myShader->SetVec3("lightColor", point_light->GetDiffuse());
		myShader->SetFloat("iso_value", iso_value);
		myShader->SetBool("is_volume", false);

		// ==================== Draw origin and 3 axes ====================
		if (Settings.ShowOriginAnd3Axes) {
			this->DrawOriginAnd3Axes(myShader.get());
		}

		if (engine->GetCurrentRenderMode() == Nexus::RENDER_MODE_ISO_SURFACE) {
			// Iso Surface
			if (engine->GetIsInitialize() && engine->GetIsReadyToDraw()) {
				model->Push();
				model->Save(glm::translate(model->Top(), engine->GetResolution() * -0.5f));
				myShader->SetVec3("objectColor", glm::vec3(0.482352941, 0.68627451, 0.929411765));
				engine->Draw(myShader.get(), model->Top());
				if (Settings.NormalVisualize) {
					normalShader->Use();
					normalShader->SetMat4("view", view);
					normalShader->SetMat4("projection", projection);
					engine->Draw(normalShader.get(), model->Top());
				}
				model->Pop();
			}
		} else if (engine->GetCurrentRenderMode() == Nexus::RENDER_MODE_RAY_CASTING) {
			// Volume Rendering: Ray Casting

			// Shader Init
			rayShader->Use();
			rayShader->SetMat4("view", view);
			rayShader->SetMat4("projection", projection);
			rayShader->SetVec3("lightPos", point_light->GetPosition());
			rayShader->SetVec3("viewPos", Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());
			rayShader->SetVec3("lightColor", point_light->GetDiffuse());
			rayShader->SetVec3("backgroundColor", this->Settings.BackgroundColor);
			rayShader->SetFloat("sample_rate", sample_rate);
			rayShader->SetBool("useNormalColor", use_normal_color);
			rayShader->SetBool("useLighting", use_lighting);

			if (engine->GetIsInitialize() && engine->GetIsReadyToDraw()) {
				model->Push();
				model->Save(glm::translate(model->Top(), engine->GetResolution() * -0.5f));
				//model->Save(glm::translate(model->Top(), glm::vec3(-149 / 2.0f, -208 / 2.0f, -110 / 2.0f)));
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D, engine->GetVolumeTexture());
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_1D, transfer_function_texture);
				engine->Draw(rayShader.get(), model->Top());
				model->Pop();
			}

			

			

			/*
			// Frame buffer binding
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glEnable(GL_DEPTH_TEST);
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			
			// Shader Init
			entryExitShader->Use();
			entryExitShader->SetMat4("view", view);
			entryExitShader->SetMat4("projection", projection);

			glDrawBuffer(GL_COLOR_ATTACHMENT1);
			
			// 設定 DepthFunc 為 GL_GREATER，代表距離比較遠的會蓋過距離比較近的物體。　（視覺上類似Front Face Culling）
			glDepthFunc(GL_GREATER);

			// 儲存當前的 GL_DEPTH_CLEAR_VALUE 值（這個是預設值 1.0f）
			float old_depth;
			glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old_depth);
			
			// 設定為 0，如此一來深度值必須大於0(比較遠)才會畫出來，如果設 1.0f 什麼都畫不出來。 （因為GL_GREATER）
			glClearDepth(0.0f);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			if (engine->GetIsInitialize() && engine->GetIsReadyToDraw()) {
				model->Push();
				engine->Draw(entryExitShader.get(), model->Top());
				model->Pop();
			}

			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			
			// 設定 DepthFunc 為 GL_LESS，代表距離比較近的會蓋過距離比較遠的物體。
			glDepthFunc(GL_LESS);

			// 將 Depth Buffer 的預設值設回原本的預設 （也就是 1.0f） （因為現在是 GL_LESS，必須要小於1.0f才畫得出東西來）
			glClearDepth(old_depth);
			
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			if (engine->GetIsInitialize() && engine->GetIsReadyToDraw()) {
				model->Push();
				engine->Draw(entryExitShader.get(), model->Top());
				model->Pop();
			}

			screenShader->Use();
			screenShader->SetInt("entry_points_sampler", 0);
			screenShader->SetInt("exit_points_sampler", 1);
			screenShader->SetInt("volume", 2);
			screenShader->SetInt("transfer_function", 3);
			screenShader->SetVec2("screen_size", glm::vec2(Settings.Width, Settings.Height));
			screenShader->SetVec3("volume_resoultion", engine->GetResolution());
			screenShader->SetFloat("sampling_rate", sampling_rate);
			screenShader->SetVec3("viewPos", Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());
			screenShader->SetVec3("lightPos", Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());
			screenShader->SetVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
			
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDisable(GL_DEPTH_TEST);
			glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, entry_points->ID);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, exit_points->ID);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_3D, engine->GetVolumeTexture());
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_1D, transfer_function_texture);
			
			// Draw screen quad
			quad->Draw(screenShader.get());
			*/
		}

		// ImGui::ShowDemoWindow();
		// ImPlot::ShowDemoWindow();
		ImGui::ShowBezierDemo();
	}

	void ShowDebugUI() override {

		ImGuiTabBarFlags tab_bar_flags = ImGuiBackendFlags_None;
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		ImGui::Begin("Transfer Function Example");
		tf.ShowUI();
		ImGui::End();
		
		if (engine->GetCurrentRenderMode() == Nexus::RENDER_MODE_RAY_CASTING) {
			// 使用 Ray Casting 才會生成 Transfer Function
			ImGui::Begin("Transfer Function");
			if (ImGui::Button("Export")) {
				Nexus::FileLoader::OutputTransferFunction("Resource/Exports/transfer.txt", tf_widget.get_colormapf(), engine.get());
			}
			tf_widget.draw_ui();
			glDeleteTextures(1, &transfer_function_texture);
			transfer_function_texture = GetTFTexture(tf_widget);
			ImGui::End();
		}

		
		ImGui::Begin("Volume Control Center");
		if (ImGui::BeginTabBar("VolumeTabBar", tab_bar_flags)) {
			if (ImGui::BeginTabItem("File Setting")) {
				ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.0f, 1.0f), "1. Input a folder path which has volume data to load in.");
				ImGui::InputText("Volume Data Folder", volume_data_folder_path, IM_ARRAYSIZE(volume_data_folder_path));
				if (ImGui::Button("Loading Folder")) {
					file_names_raw = Nexus::FileLoader::GetAllFilesNamesWithinFolder(std::string(volume_data_folder_path), "/*.raw");
					file_names_inf = Nexus::FileLoader::GetAllFilesNamesWithinFolder(std::string(volume_data_folder_path), "/*.inf");

					if (file_names_raw.empty() || file_names_inf.empty()) {
						Nexus::Logger::Message(Nexus::LOG_WARNING, "There is no any volume data (.raw and .inf) files in this folder: ");
						Nexus::Logger::Message(Nexus::LOG_WARNING, std::string(volume_data_folder_path));
						Nexus::Logger::Message(Nexus::LOG_WARNING, "Please try another folder path to load.");
						current_item_raw = "none";
						current_item_inf = "none";
						ImGui::OpenPopup("Error##01");
					} else {
						Nexus::Logger::Message(Nexus::LOG_INFO, "Folder Path: " + std::string(volume_data_folder_path));
						Nexus::Logger::Message(Nexus::LOG_INFO, "Found " + std::to_string(file_names_raw.size()) + " raw files.");
						Nexus::Logger::Message(Nexus::LOG_INFO, "Found " + std::to_string(file_names_inf.size()) + " inf files.");
						current_item_raw = "Please select a file...";
						current_item_inf = "Please select a file...";
					}
				}
				
				if (ImGui::BeginPopupModal("Error##01", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("There is no any volume data (.raw and .inf) files in this folder:\n");
					ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.0f, 1.0f), volume_data_folder_path);
					ImGui::Text("Please try another folder path to load.\n\n");
					ImGui::Separator();

					if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
					ImGui::SetItemDefaultFocus();
					ImGui::EndPopup();
				}
				ImGui::Spacing();
				ImGui::Separator();

				ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.0f, 1.0f), "2. Select a volume data (raw and inf) to load.");
				if (ImGui::BeginCombo("Volume Data (Raw)", current_item_raw.c_str())) {
					for (int n = 0; n < file_names_raw.size(); n++) {
						bool is_selected = (current_item_raw == file_names_raw[n]);
						if (ImGui::Selectable(file_names_raw[n].c_str(), is_selected)) {
							current_item_raw = file_names_raw[n];
						}
						if (is_selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::BeginCombo("Volume Data (Inf)", current_item_inf.c_str())) {
					for (int n = 0; n < file_names_inf.size(); n++) {
						bool is_selected = (current_item_inf == file_names_inf[n]);
						if (ImGui::Selectable(file_names_inf[n].c_str(), is_selected)) {
							current_item_inf = file_names_inf[n];
						}
						if (is_selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				ImGui::SliderFloat("Gradient Threshold", &max_gradient, 1.0f, 400.0f);
				if (ImGui::Button("Loading Files")) {
					if (file_names_raw.empty() || file_names_inf.empty() || current_item_raw == "Please select a file..." || current_item_inf == "Please select a file..." || current_item_raw == "none" || current_item_inf == "none") {
						Nexus::Logger::Message(Nexus::LOG_ERROR, "Please select a folder path and choose a volume data first!");
						ImGui::OpenPopup("Error##02");
					} else {
						// 初始化
						engine->Initialize(std::string(volume_data_folder_path) + "/" + current_item_inf, std::string(volume_data_folder_path) + "/" + current_item_raw, max_gradient);

						iso_value_histogram = engine->GetIsoValueHistogram();
						iso_value_histogram_max = *std::max_element(iso_value_histogram.cbegin(), iso_value_histogram.cend());

						gradient_histogram = engine->GetGradientHistogram();
						gradient_histogram_max = *std::max_element(gradient_histogram.cbegin(), gradient_histogram.cend());

						gradient_heatmap = engine->GetGradientHeatmap();
						gradient_heatmap_max = 140.0f;
						gradient_heatmap_min = 0.0f;
						
						engine->GetGradientHeatmapAxisLabels(gradient_heatmap_labelx_string, true);
						engine->GetGradientHeatmapAxisLabels(gradient_heatmap_labely_string, false);

						gradient_heatmap_labelx.clear();
						for (int i = 0; i < gradient_heatmap_labelx_string.size(); i++) {
							gradient_heatmap_labelx.push_back(gradient_heatmap_labelx_string[i].c_str());
						}
						gradient_heatmap_labely.clear();
						for (int i = gradient_heatmap_labely_string.size() - 1; i >=0; i--) {
							gradient_heatmap_labely.push_back(gradient_heatmap_labely_string[i].c_str());
						}
						
					}
				}
				ImGui::Spacing();
				
				if (engine->GetIsInitialize()) {
					// 顯示所讀取的 raw 和 inf 資料
					if (ImGui::CollapsingHeader("Volume Data Attributes")) {
						ImGui::BulletText("Resolution: %.2f, %.2f, %.2f", engine->GetResolution().x, engine->GetResolution().y, engine->GetResolution().z);
						ImGui::BulletText("Ratio: %.2f, %.2f, %.2f", engine->GetRatio().x, engine->GetRatio().y, engine->GetRatio().z);
						ImGui::BulletText("DataType: %s", engine->GetDataType());
						ImGui::BulletText("Endian: %s", engine->GetEndian());
					}
					if (ImGui::CollapsingHeader("Gradient Histogram")) {
						ImGui::PlotHistogram("Gradient Histogram", gradient_histogram.data(), gradient_histogram.size(), 0, NULL, 0.0f, gradient_histogram_max, ImVec2(0, 300));
					}
					if (ImGui::CollapsingHeader("Gradient Heatmaps")) {
						static ImPlotColormap map = ImPlotColormap_Viridis;
						if (ImPlot::ColormapButton(ImPlot::GetColormapName(map), ImVec2(512, 0), map)) {
							map = (map + 1) % ImPlot::GetColormapCount();
							ImPlot::BustColorCache("##Heatmap1");
						}
						ImGui::SameLine();
						ImGui::LabelText("##Colormap Index", "%s", "Change Colormap");

						ImGui::SetNextItemWidth(512);
						ImGui::DragFloatRange2("Min / Max", &gradient_heatmap_min, &gradient_heatmap_max, 10.0f, 0, 20000);


						
						static ImPlotAxisFlags axes_flags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;
						ImPlot::PushColormap(map);
						ImPlot::SetNextPlotTicksX(0, 1, 5, gradient_heatmap_labelx.data());
						ImPlot::SetNextPlotTicksY(1, 0, 5, gradient_heatmap_labely.data());
						if (ImPlot::BeginPlot("##Heatmap1", NULL, NULL, ImVec2(512, 512), ImPlotFlags_NoLegend, axes_flags, axes_flags)) {
							ImPlot::PlotHeatmap("heat", gradient_heatmap.data(), engine->Interval, engine->Interval, gradient_heatmap_min, gradient_heatmap_max, NULL);
							ImPlot::EndPlot();
						}
						ImGui::SameLine();
						ImPlot::ColormapScale("##HeatScale", gradient_heatmap_min, gradient_heatmap_max, ImVec2(60, 512));
						ImPlot::PopColormap();
					}
				}
				
				if (ImGui::BeginPopupModal("Error##02", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("Please select a folder path and choose a volume data first!\n\n");
					ImGui::Separator();

					if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
					ImGui::SetItemDefaultFocus();
					ImGui::EndPopup();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Volume Setting")) {
				if (engine->GetIsInitialize()) {
					if (ImGui::CollapsingHeader("Histograms")) {
						ImGui::PlotHistogram("Histogram", iso_value_histogram.data(), iso_value_histogram.size(), 0, NULL, 0.0f, iso_value_histogram_max, ImVec2(0, 300));
					}
					if (ImGui::Button("Equalization")) {
						engine->IsoValueHistogramEqualization();
						iso_value_histogram = engine->GetIsoValueHistogram();

						engine->GenerateGradientHeatMap();
						gradient_heatmap = engine->GetGradientHeatmap();
						gradient_heatmap_max = 140.0f;
						gradient_heatmap_min = 0.0f;
					}
					ImGui::Spacing();
					ImGui::Separator();

					if (ImGui::BeginCombo("Render Mode", current_render_mode.c_str())) {
						for (int n = 0; n < render_modes.size(); n++) {
							bool is_selected = (current_render_mode == render_modes[n]);
							if (ImGui::Selectable(render_modes[n].c_str(), is_selected)) {
								current_render_mode = render_modes[n];
								switch (n) {
								case 0:
									engine->SetCurrentRenderMode(Nexus::RENDER_MODE_ISO_SURFACE);
									break;
								case 1:
									engine->SetCurrentRenderMode(Nexus::RENDER_MODE_RAY_CASTING);
									break;
								}
							}
							if (is_selected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::Spacing();
					ImGui::Separator();

					if (engine->GetCurrentRenderMode() == Nexus::RENDER_MODE_ISO_SURFACE) {
						ImGui::SliderFloat("Iso Value", &iso_value, 0, 255);
						if (ImGui::Button("Generate")) {
							if (engine->GetIsInitialize()) {
								engine->SetIsoValue(iso_value);
								engine->ConvertToPolygon();
								engine->Debug();
								// iso_value_shader = iso_value;
							} else {
								Nexus::Logger::Message(Nexus::LOG_ERROR, "YOU MUST LOAD THE VOLUME DATA FIRST and COMPUTE THESE ISO SURFACE VERTICES.");
							}
						}
						ImGui::Spacing();
						ImGui::Separator();
						ImGui::Checkbox("Normal Visualize", &Settings.NormalVisualize);
						ImGui::SameLine();
						ImGui::Checkbox("Wire Frame Mode", engine->WireFrameModeHelper());
					} else if (engine->GetCurrentRenderMode() == Nexus::RENDER_MODE_RAY_CASTING) {
						ImGui::SliderFloat("Sample Rate", &sample_rate, 0.01, 1);
						ImGui::Checkbox("Normal Color", &use_normal_color);
						ImGui::Checkbox("Lighting", &use_lighting);
						if (ImGui::Button("Generate")) {
							if (engine->GetIsInitialize()) {
								engine->ConvertToPolygon();
								engine->Debug();
							} else {
								Nexus::Logger::Message(Nexus::LOG_ERROR, "YOU MUST LOAD THE VOLUME DATA FIRST and COMPUTE THESE ISO SURFACE VERTICES.");
							}
						}
					}
					
					
				} else {
					ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.0f, 1.0f), "You must load the volume data first and compute these iso surface vertices.");
				}
				
				
				
				ImGui::EndTabItem();
			}
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::End();

		ImGui::Begin("General Setting");
		if (ImGui::BeginTabBar("GeneralTabBar", tab_bar_flags)) {

			if (ImGui::BeginTabItem("Camera")) {
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
				if (Settings.EnableGhostMode) {
					first_camera->ShowDebugUI("First Person Camera");
				} else {
					third_camera->ShowDebugUI("Third Person Camera");
					ImGui::Text("Distance: %.2f", third_camera->GetDistance());
				}
				if (ImGui::Button("Reset Position")) {
					if (Settings.EnableGhostMode) {
						first_camera->SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
						first_camera->SetPitch(0.0f);
						first_camera->SetYaw(0.0f);
					} else {
						third_camera->SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
						third_camera->SetPitch(0.0f);
						third_camera->SetYaw(0.0f);
					}
					
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Projection")) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), (ProjectionSettings.IsPerspective) ? "Perspective Projection" : "Orthogonal Projection");
				ImGui::Text("Parameters");
				ImGui::BulletText("FoV = %.2f deg, Aspect = %.2f", Settings.EnableGhostMode? first_camera->GetFOV() : third_camera->GetFOV(), ProjectionSettings.Aspect);
				if(!ProjectionSettings.IsPerspective) {
					ImGui::SliderFloat("Length", &ProjectionSettings.OrthogonalHeight, 100.0f, 1000.0f);
				}
				// ImGui::BulletText("left: %.2f, right: %.2f ", ProjectionSettings.ClippingLeft, ProjectionSettings.ClippingTop);
				// ImGui::BulletText("bottom: %.2f, top: %.2f ", ProjectionSettings.ClippingBottom, ProjectionSettings.ClippingTop);
				ImGui::DragFloatRange2("Near & Far", &ProjectionSettings.ClippingNear, &ProjectionSettings.ClippingFar, 0.1f, 0.1f, 500.0f);
				ImGui::Spacing();

				if (ImGui::TreeNode("Projection Matrix")) {
					SetProjectionMatrix(Nexus::DISPLAY_MODE_DEFAULT);
					glm::mat4 proj = projection;

					ImGui::Columns(4, "mycolumns");
					ImGui::Separator();
					for (int i = 0; i < 4; i++) {
						ImGui::Text("%.2f", proj[0][i]); ImGui::NextColumn();
						ImGui::Text("%.2f", proj[1][i]); ImGui::NextColumn();
						ImGui::Text("%.2f", proj[2][i]); ImGui::NextColumn();
						ImGui::Text("%.2f", proj[3][i]); ImGui::NextColumn();
						ImGui::Separator();
					}
					ImGui::Columns(1);

					ImGui::TreePop();
				}
				ImGui::Spacing();
				/*
				if (ImGui::TreeNode("View Volume Vertices")) {
					view_volume->ShowViewVolumeVerticesImGUI();
					ImGui::TreePop();
				}
				ImGui::Spacing();

				if (ImGui::TreeNode("View Volume Normals")) {
					view_volume->ShowViewVolumeNormalsImGUI();
					ImGui::TreePop();
				}
				ImGui::Spacing();
				*/
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Light Setting")) {
				// 新增 Light Position
				ImGui::SliderFloat3("Light Color", point_light->GetDiffusePointer(), 0.0f, 1.0f);

				ImGui::EndTabItem();
			}
			
			if (ImGui::BeginTabItem("Illustration")) {
				// ImGui::Text("Current Screen: %d", Settings.CurrentDisplyMode);
				// ImGui::Text("Full Screen:  %s", isfullscreen ? "True" : "false");
				ImGui::Text("Showing Axes: %s", Settings.ShowOriginAnd3Axes ? "True" : "false");

				ImGui::Spacing();
				ImGui::Checkbox("Face Culling", &Settings.EnableFaceCulling);
				if (ImGui::BeginCombo("Culling Type", Settings.CullingTypeStr.c_str())) {
					for (int n = 0; n < Settings.CullingTypes.size(); n++) {
						bool is_selected = (Settings.CullingTypeStr == Settings.CullingTypes[n]);
						if (ImGui::Selectable(Settings.CullingTypes[n].c_str(), is_selected)) {
							Settings.CullingTypeStr = Settings.CullingTypes[n];
						}
						if (is_selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				
				
				ImGui::SliderFloat3("Background Color", glm::value_ptr(this->Settings.BackgroundColor), 0.0f, 1.0f);
				ImGui::Spacing();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::End();
	}

	void DrawOriginAnd3Axes(Nexus::Shader* shader) const {
		// 繪製世界坐標系原點（0, 0, 0）
		model->Push();
		model->Save(glm::scale(model->Top(), glm::vec3(0.8f, 0.8f, 0.8f)));
		shader->SetVec3("objectColor", glm::vec3(0.2f, 0.2f, 0.2f));
		sphere->Draw(shader, model->Top());
		model->Pop();

		// 繪製三個軸
		model->Push();
		model->Push();
		model->Save(glm::translate(model->Top(), glm::vec3(100.0f, 0.0f, 0.0f)));
		model->Save(glm::scale(model->Top(), glm::vec3(200.0f, 1.0f, 1.0f)));
		shader->SetVec3("objectColor", glm::vec3(1.0f, 0.0f, 0.0f));
		cube->Draw(shader, model->Top());
		model->Pop();

		model->Push();
		model->Save(glm::translate(model->Top(), glm::vec3(0.0f, 100.0f, 0.0f)));
		model->Save(glm::scale(model->Top(), glm::vec3(1.0f, 200.0f, 1.0f)));
		shader->SetVec3("objectColor", glm::vec3(0.0f, 1.0f, 0.0f));
		cube->Draw(shader, model->Top());
		model->Pop();

		model->Push();
		model->Save(glm::translate(model->Top(), glm::vec3(0.0f, 0.0f, 100.0f)));
		model->Save(glm::scale(model->Top(), glm::vec3(1.0f, 1.0f, 200.0f)));
		shader->SetVec3("objectColor", glm::vec3(0.0f, 0.0f, 1.0f));
		cube->Draw(shader, model->Top());
		model->Pop();
		model->Pop();
	}

	void SetViewMatrix(Nexus::DisplayMode monitor_type) {
		glm::vec3 camera_position = Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition();
		switch (monitor_type) {
			case Nexus::DISPLAY_MODE_ORTHOGONAL_X:
				view = glm::lookAt(camera_position + glm::vec3(5.0, 0.0, 0.0), camera_position, glm::vec3(0.0, 1.0, 0.0));
				break;
			case Nexus::DISPLAY_MODE_ORTHOGONAL_Y:
				view = glm::lookAt(camera_position + glm::vec3(0.0, 5.0, 0.0), camera_position, glm::vec3(0.0, 0.0, -1.0));
				break;
			case Nexus::DISPLAY_MODE_ORTHOGONAL_Z:
				view = glm::lookAt(camera_position + glm::vec3(0.0, 0.0, 5.0), camera_position, glm::vec3(0.0, 1.0, 0.0));
				break;
			case Nexus::DISPLAY_MODE_DEFAULT:
				view = Settings.EnableGhostMode ? first_camera->GetViewMatrix() : third_camera->GetViewMatrix();
				break;
		}
	}

	void SetProjectionMatrix(Nexus::DisplayMode monitor_type) {
		ProjectionSettings.Aspect = (float)Settings.Width / (float)Settings.Height;

		if (monitor_type == Nexus::DISPLAY_MODE_DEFAULT) {
			if (ProjectionSettings.IsPerspective) {
				projection = GetPerspectiveProjMatrix(glm::radians(Settings.EnableGhostMode ? first_camera->GetFOV() : third_camera->GetFOV()), ProjectionSettings.Aspect, ProjectionSettings.ClippingNear, ProjectionSettings.ClippingFar);
			} else {
				projection = GetOrthoProjMatrix(-ProjectionSettings.OrthogonalHeight * ProjectionSettings.Aspect, ProjectionSettings.OrthogonalHeight * ProjectionSettings.Aspect, -ProjectionSettings.OrthogonalHeight, ProjectionSettings.OrthogonalHeight, ProjectionSettings.ClippingNear, ProjectionSettings.ClippingFar);
			}
		} else {
			projection = GetOrthoProjMatrix(-100.0f * ProjectionSettings.Aspect, 100.0f * ProjectionSettings.Aspect, -100.0f, 100.0f, ProjectionSettings.ClippingNear, ProjectionSettings.ClippingFar);
		}
	}

	void SetViewport(Nexus::DisplayMode monitor_type) override {
		if (Settings.CurrentDisplyMode == Nexus::DISPLAY_MODE_3O1P) {
			switch (monitor_type) {
			case Nexus::DISPLAY_MODE_ORTHOGONAL_X:
				glViewport(0, Settings.Height / 2, Settings.Width / 2, Settings.Height / 2);
				break;
			case Nexus::DISPLAY_MODE_ORTHOGONAL_Y:
				glViewport(Settings.Width / 2, Settings.Height / 2, Settings.Width / 2, Settings.Height / 2);
				break;
			case Nexus::DISPLAY_MODE_ORTHOGONAL_Z:
				glViewport(0, 0, Settings.Width / 2, Settings.Height / 2);
				break;
			case Nexus::DISPLAY_MODE_DEFAULT:
				glViewport(Settings.Width / 2, 0, Settings.Width / 2, Settings.Height / 2);
				break;
			}
		} else {
			glViewport(0, 0, Settings.Width, Settings.Height);
		}
	}

	void OnWindowResize() override {
		ProjectionSettings.Aspect = (float)Settings.Width / (float)Settings.Height;
	}

	void OnProcessInput(int key) override {
		if (Settings.EnableGhostMode) {
			if (key == GLFW_KEY_W) {
				first_camera->ProcessKeyboard(Nexus::CAMERA_FORWARD, DeltaTime);
			}
			if (key == GLFW_KEY_S) {
				first_camera->ProcessKeyboard(Nexus::CAMERA_BACKWARD, DeltaTime);
			}
			if (key == GLFW_KEY_A) {
				first_camera->ProcessKeyboard(Nexus::CAMERA_LEFT, DeltaTime);
			}
			if (key == GLFW_KEY_D) {
				first_camera->ProcessKeyboard(Nexus::CAMERA_RIGHT, DeltaTime);
			}
		}
	}
	
	void OnKeyPress(int key) override {
		if (key == GLFW_KEY_LEFT_SHIFT) {
			if (Settings.EnableGhostMode) {
				first_camera->SetMovementSpeed(500.0f);
			}
		}

		if (key == GLFW_KEY_X) {
			if (Settings.ShowOriginAnd3Axes) {
				Settings.ShowOriginAnd3Axes = false;
				Nexus::Logger::Message(Nexus::LOG_INFO, "World coordinate origin and 3 axes: [Hide]");
			} else {
				Settings.ShowOriginAnd3Axes = true;
				Nexus::Logger::Message(Nexus::LOG_INFO, "World coordinate origin and 3 axes: [Show]");
			}
		}

		if (key == GLFW_KEY_P) {
			if (ProjectionSettings.IsPerspective) {
				ProjectionSettings.IsPerspective = false;
				Nexus::Logger::Message(Nexus::LOG_INFO, "Projection Mode: Orthogonal");
			} else {
				ProjectionSettings.IsPerspective = true;
				Nexus::Logger::Message(Nexus::LOG_INFO, "Projection Mode: Perspective");
			}
		}

		if (key == GLFW_KEY_G) {
			if (Settings.EnableGhostMode) {
				Settings.EnableGhostMode = false;
				Nexus::Logger::Message(Nexus::LOG_INFO, "Camera Mode: Third Person");
			} else {
				Settings.EnableGhostMode = true;
				Nexus::Logger::Message(Nexus::LOG_INFO, "Camera Mode: First Person");
			}
		}
	}

	void OnKeyRelease(int key) override {
		if (key == GLFW_KEY_LEFT_SHIFT) {
			if (Settings.EnableGhostMode) {
				first_camera->SetMovementSpeed(50.0f);
			}
		}
	}

	void OnMouseMove(int xoffset, int yoffset) override {
		if (!Settings.EnableCursor) {
			if (Settings.EnableGhostMode) {
				first_camera->ProcessMouseMovement(xoffset, yoffset);
			} else {
				third_camera->ProcessMouseMovement(xoffset, yoffset);
			}
		}
	}

	void OnMouseButtonPress(int button) override {
		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			SetCursorDisable(true);
			Settings.EnableCursor = false;
		}
	}

	void OnMouseButtonRelease(int button) override {
		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			SetCursorDisable(false);
			Settings.EnableCursor = true;
		}
	}

	void OnMouseScroll(int yoffset) override {
		if (ProjectionSettings.IsPerspective) {
			if (Settings.EnableGhostMode) {
				first_camera->ProcessMouseScroll(yoffset);
			} else {
				third_camera->AdjustDistance(yoffset);
			}
		} else {
			AdjustOrthogonalProjectionWidth(yoffset);
		}
	}

	void AdjustOrthogonalProjectionWidth(float yoffset) {
		if (ProjectionSettings.OrthogonalHeight >= 100.0f && ProjectionSettings.OrthogonalHeight <= 1000.0f) {
			ProjectionSettings.OrthogonalHeight -= (float)yoffset * 10.0f;
		}
		if (ProjectionSettings.OrthogonalHeight < 100.0f) {
			ProjectionSettings.OrthogonalHeight = 100.0f;
		}
		if (ProjectionSettings.OrthogonalHeight > 1000.0f) {
			ProjectionSettings.OrthogonalHeight = 1000.0f;
		}
	}

	GLuint GetTFTexture(TransferFunctionWidget& tf_widget) {
		const std::vector<float>& colormap = tf_widget.get_colormapf();
		const size_t texel_count = colormap.size() / 4;

		// Make it into a 1D texture
		GLuint texture;
		glGenTextures(1, &texture);

		glBindTexture(GL_TEXTURE_1D, texture);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		// Upload it to the GPU
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, texel_count, 0, GL_RGBA, GL_FLOAT, colormap.data());

		glBindTexture(GL_TEXTURE_1D, 0);

		return texture;
	}
	
private:
	std::unique_ptr<Nexus::Shader> myShader = nullptr;
	std::unique_ptr<Nexus::Shader> rayShader = nullptr;
	std::unique_ptr<Nexus::Shader> normalShader = nullptr;
	std::unique_ptr<Nexus::Shader> screenShader = nullptr;

	std::unique_ptr<Nexus::FirstPersonCamera> first_camera = nullptr;
	std::unique_ptr<Nexus::ThirdPersonCamera> third_camera = nullptr;

	std::unique_ptr<Nexus::MatrixStack> model = nullptr;
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);

	std::unique_ptr<Nexus::Cube> cube = nullptr;
	std::unique_ptr<Nexus::NDCQuad> quad = nullptr;
	std::unique_ptr<Nexus::Sphere> sphere = nullptr;

	std::unique_ptr<Nexus::IsoSurface> engine = nullptr;

	std::unique_ptr<Nexus::PointLight> point_light;

	bool is_loaded_file = false;
	char volume_data_folder_path[128] = "Resource/VolumeData";
	std::vector<std::string> file_names_raw;
	std::vector<std::string> file_names_inf;
	std::string current_item_raw = "none";
	std::string current_item_inf = "none";
	std::vector<std::string> render_modes = { "Iso Surface", "Ray Casting" };
	std::string current_render_mode = "Iso Surface";

	std::vector<float> iso_value_histogram;
	std::vector<float> gradient_histogram;
	std::vector<float> gradient_heatmap;
	std::vector<const char*> gradient_heatmap_labelx;
	std::vector<const char*> gradient_heatmap_labely;
	std::vector<std::string> gradient_heatmap_labelx_string;
	std::vector<std::string> gradient_heatmap_labely_string;
	float iso_value = 80.0;
	float max_gradient = 300.0f;
	float iso_value_histogram_max;
	float gradient_histogram_max;
	float gradient_heatmap_max;
	float gradient_heatmap_min;
	float sample_rate = 0.5f;
	bool use_normal_color = false;
	bool use_lighting = true;
	
	// bool enable_transfer_function = false;
	TransferFunctionWidget tf_widget;
	GLuint transfer_function_texture;

	TransferFunction tf;

	GLuint framebuffer, rbo;
	std::unique_ptr<Nexus::Texture2D> texture_color_buffer = nullptr;
};

int main() {
	VolumeRendering app;
	return app.Run();
}