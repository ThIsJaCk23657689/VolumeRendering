#include <imgui.h>

#include "Application.h"
#include "Logger.h"
#include "FirsrPersonCamera.h"
#include "ThirdPersonCamera.h"
#include "Shader.h"
#include "MatrixStack.h"
#include "IsoSurface.h"

#include "Cube.h"
#include "Sphere.h"
#include <algorithm>
#include <random>

class VolumeRendering final : public Nexus::Application {
public:
	VolumeRendering() {
		Settings.Width = 800;
		Settings.Height = 600;
		Settings.WindowTitle = "Iso Surface | Nexus";
		Settings.EnableDebugCallback = true;
		Settings.EnableFullScreen = false;

		Settings.EnableGhostMode = false;
		Settings.ShowOriginAnd3Axes = false;

		// Projection Settings Initalize
		ProjectionSettings.IsPerspective = true;
		ProjectionSettings.ClippingNear = 0.1f;
		ProjectionSettings.ClippingFar = 500.0f;
		ProjectionSettings.Aspect = static_cast<float>(Settings.Width) / static_cast<float>(Settings.Height);
	}

	void Initialize() override {
		// Setting OpenGL
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Create shader program
		myShader = std::make_unique<Nexus::Shader>("Shaders/simple_lighting.vert", "Shaders/simple_lighting.frag");
		normalShader = std::make_unique<Nexus::Shader>("Shaders/normal_visualization.vs", "Shaders/normal_visualization.fs", "Shaders/normal_visualization.gs");

		// Create Camera
		first_camera = std::make_unique<Nexus::FirstPersonCamera>(glm::vec3(0.0f, 0.0f, 430.0f));
		third_camera = std::make_unique<Nexus::ThirdPersonCamera>(glm::vec3(0.0f, 0.0f, 430.0f));

		// Create Matrix Stack
		model = std::make_unique<Nexus::MatrixStack>();

		// Create object data
		engine = std::make_unique<Nexus::IsoSurface>();
		engine_2 = std::make_unique<Nexus::IsoSurface>();
		
		cube = std::make_unique<Nexus::Cube>();
		sphere = std::make_unique<Nexus::Sphere>();
	}

	void Update(Nexus::DisplayMode monitor_type) override {

		SetViewMatrix(Nexus::DISPLAY_MODE_DEFAULT);
		SetProjectionMatrix(Nexus::DISPLAY_MODE_DEFAULT);
		SetViewport(Nexus::DISPLAY_MODE_DEFAULT);

		myShader->Use();
		myShader->SetMat4("view", view);
		myShader->SetMat4("projection", projection);
		myShader->SetVec3("lightPos", Settings.EnableGhostMode? first_camera->GetPosition() : third_camera->GetPosition());
		myShader->SetVec3("viewPos", Settings.EnableGhostMode ? first_camera->GetPosition() : third_camera->GetPosition());
		myShader->SetVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

		// ==================== Draw origin and 3 axes ====================
		if (Settings.ShowOriginAnd3Axes) {
			this->DrawOriginAnd3Axes(myShader.get());
		}

		if (engine->GetIsInitialize() && engine->GetIsReadyToDraw()) {
			model->Push();
			model->Save(glm::translate(model->Top(), glm::vec3(-149 / 2.0f, -208 / 2.0f, -110 / 2.0f)));
			myShader->SetVec3("objectColor", glm::vec3(0.482352941, 0.68627451, 0.929411765));
			engine->Draw(myShader.get(), model->Top());
			myShader->SetVec3("objectColor", glm::vec3(0.929411765, 0.68627451, 0.482352941));
			engine_2->Draw(myShader.get(), model->Top());
			if (Settings.NormalVisualize) {	
				normalShader->Use();
				normalShader->SetMat4("view", view);
				normalShader->SetMat4("projection", projection);
				// model->Save(glm::translate(model->Top(), glm::vec3(-RESOLUTION_X / 2.0f, -RESOLUTION_Y / 2.0f, -RESOLUTION_Z / 2.0f)));
				engine->Draw(normalShader.get(), model->Top());
			}
			model->Pop();
		}

		// ImGui::ShowDemoWindow();
	}

	void ShowDebugUI() override {
		ImGui::Begin("Iso Surface");
		ImGui::InputText("Volume Data Path", (char*)volume_data_folder_path.c_str(), sizeof(volume_data_folder_path));
		ImGui::InputText("Volume Data Name", (char*)volume_data_name.c_str(), sizeof(volume_data_folder_path));
		if (ImGui::Button("Loading Files")) {
			engine->Initialize(volume_data_folder_path + "/" + volume_data_name + ".inf", volume_data_folder_path + "/" + volume_data_name + ".raw");
			engine_2->Initialize(volume_data_folder_path + "/" + volume_data_name + ".inf", volume_data_folder_path + "/" + volume_data_name + ".raw");
			volume_data_histogram = engine->GetnHistogramData();
			volume_data_max = *std::max_element(volume_data_histogram.cbegin(), volume_data_histogram.cend());
		
			/*
			unsigned int total = 0;
			for (auto i : volume_data_histogram) {
				std::cout << i << std::endl;
				total += i;
			}
			std::cout << total << std::endl;
			*/
		}
		
		ImGui::SliderFloat("Iso Value", &iso_value, 0, 255);
		if (ImGui::Button("Generate")) {
			if (engine->GetIsInitialize()) {
				engine->ConvertToPolygon(iso_value);
				engine->Debug();
				engine_2->ConvertToPolygon(250.0f);
				engine_2->Debug();
			} else {
				Nexus::Logger::Message(Nexus::LOG_ERROR, "YOU MUST LOAD THE VOLUME DATA FIRST and COMPUTE THESE ISO SURFACE VERTICES.");
			}
		}
		ImGui::Spacing();

		ImGui::Checkbox("Normal Visualize", &Settings.NormalVisualize);
		ImGui::SameLine();
		ImGui::Checkbox("Wire Frame Mode", engine->WireFrameModeHelper());
		if (engine->GetIsInitialize()) {
			if (ImGui::CollapsingHeader("Histograms")) {
				ImGui::PlotHistogram("Histogram", volume_data_histogram.data(), volume_data_histogram.size(), 0, NULL, 0.0f, volume_data_max, ImVec2(0, 300));
			}
		}
		ImGui::End();
		
		ImGui::Begin("Setting");
		ImGuiTabBarFlags tab_bar_flags = ImGuiBackendFlags_None;
		if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
			if (ImGui::BeginTabItem("Camera")) {
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
				if (Settings.EnableGhostMode) {
					first_camera->ShowDebugUI("Person Person Camera");
				} else {
					third_camera->ShowDebugUI("Third Person Camera");
					ImGui::BulletText("Distance: %.2f", third_camera->GetDistance());
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
				ImGui::BulletText("left: %.2f, right: %.2f ", ProjectionSettings.ClippingLeft, ProjectionSettings.ClippingTop);
				ImGui::BulletText("bottom: %.2f, top: %.2f ", ProjectionSettings.ClippingBottom, ProjectionSettings.ClippingTop);
				ImGui::SliderFloat("Near", &ProjectionSettings.ClippingNear, 0.001f, 10.0f);
				ImGui::SliderFloat("Far", &ProjectionSettings.ClippingFar, 10.0f, 1000.0f);
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
					ImGui::BulletText("rtnp: (%.2f, %.2f, %.2f)", nearPlaneVertex[0].x, nearPlaneVertex[0].y, nearPlaneVertex[0].z);
					ImGui::BulletText("ltnp: (%.2f, %.2f, %.2f)", nearPlaneVertex[1].x, nearPlaneVertex[1].y, nearPlaneVertex[1].z);
					ImGui::BulletText("rbnp: (%.2f, %.2f, %.2f)", nearPlaneVertex[2].x, nearPlaneVertex[2].y, nearPlaneVertex[2].z);
					ImGui::BulletText("lbnp: (%.2f, %.2f, %.2f)", nearPlaneVertex[3].x, nearPlaneVertex[3].y, nearPlaneVertex[3].z);
					ImGui::BulletText("rtfp: (%.2f, %.2f, %.2f)", farPlaneVertex[0].x, farPlaneVertex[0].y, farPlaneVertex[0].z);
					ImGui::BulletText("ltfp: (%.2f, %.2f, %.2f)", farPlaneVertex[1].x, farPlaneVertex[1].y, farPlaneVertex[1].z);
					ImGui::BulletText("rbfp: (%.2f, %.2f, %.2f)", farPlaneVertex[2].x, farPlaneVertex[2].y, farPlaneVertex[2].z);
					ImGui::BulletText("lbfp: (%.2f, %.2f, %.2f)", farPlaneVertex[3].x, farPlaneVertex[3].y, farPlaneVertex[3].z);
					ImGui::TreePop();
				}

				ImGui::Spacing();
				*/
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Illustration")) {

				// ImGui::Text("Current Screen: %d", currentScreen + 1);
				ImGui::Text("Ghost Mode: %s", Settings.EnableGhostMode ? "True" : "false");
				// ImGui::Text("Projection Mode: %s", isPerspective ? "Perspective" : "Orthogonal");
				ImGui::Text("Showing Axes: %s", Settings.ShowOriginAnd3Axes ? "True" : "false");
				// ImGui::Text("Full Screen:  %s", isfullscreen ? "True" : "false");
				// ImGui::SliderFloat("zoom", &distanceOrthoCamera, 5, 25);
				ImGui::Spacing();

				if (ImGui::TreeNode("General")) {
					ImGui::BulletText("Press G to switch Ghost Mode");
					ImGui::BulletText("Press X to show / hide the axes");
					ImGui::BulletText("Press Y to switch the projection");
					ImGui::BulletText("Press 1~5 to switch the screen");
					ImGui::BulletText("Press F11 to Full Screen");
					ImGui::BulletText("Press ESC to close the program");
					ImGui::TreePop();
				}
				ImGui::Spacing();

				if (ImGui::TreeNode("Ghost Camera Illustration")) {
					ImGui::BulletText("WSAD to move camera");
					ImGui::BulletText("Hold mouse right button to rotate");
					ImGui::BulletText("Press Left Shift to speed up");
					ImGui::TreePop();
				}
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
		// ø�s�@�ɧ��Шt���I�]0, 0, 0�^
		model->Push();
		model->Save(glm::scale(model->Top(), glm::vec3(0.8f, 0.8f, 0.8f)));
		shader->SetVec3("objectColor", glm::vec3(0.2f, 0.2f, 0.2f));
		sphere->Draw(shader, model->Top());
		model->Pop();

		// ø�s�T�Ӷb
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
	
private:
	std::unique_ptr<Nexus::Shader> myShader = nullptr;
	std::unique_ptr<Nexus::Shader> normalShader = nullptr;

	std::unique_ptr<Nexus::FirstPersonCamera> first_camera = nullptr;
	std::unique_ptr<Nexus::ThirdPersonCamera> third_camera = nullptr;

	std::unique_ptr<Nexus::MatrixStack> model = nullptr;
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);

	std::unique_ptr<Nexus::Cube> cube = nullptr;
	std::unique_ptr<Nexus::Sphere> sphere = nullptr;

	std::unique_ptr<Nexus::IsoSurface> engine = nullptr;
	std::unique_ptr<Nexus::IsoSurface> engine_2 = nullptr;

	bool is_loaded_file = false;
	std::string volume_data_folder_path = "C:/Users/user/Desktop/OpenGL/Scalar";
	std::string volume_data_name = "engine";
	std::vector<float> volume_data_histogram;
	float volume_data_max;
	float iso_value = 80.0;
};

int main() {
	VolumeRendering app;
	return app.Run();
}