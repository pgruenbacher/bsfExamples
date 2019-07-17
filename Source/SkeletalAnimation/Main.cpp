// Framework includes
#include "BsApplication.h"
#include "Resources/BsResources.h"
#include "Resources/BsBuiltinResources.h"
#include "Material/BsMaterial.h"
#include "Components/BsCCamera.h"
#include "Components/BsCRenderable.h"
#include "Components/BsCAnimation.h"
#include "Components/BsCSkybox.h"
#include "RenderAPI/BsRenderAPI.h"
#include "RenderAPI/BsRenderWindow.h"
#include "Scene/BsSceneObject.h"
#include "CoreThread/BsCoreThread.h"
#include "RenderAPI/BsVertexDataDesc.h"
#include "RenderAPI/BsVertexData.h"

// Example includes
#include "BsCameraFlyer.h"
#include "BsExampleFramework.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates how to animate a 3D model using skeletal animation. Aside from animation this example is
// structurally similar to PhysicallyBasedShading example.
//
// The example first loads necessary resources, including a mesh and textures to use for rendering, as well as an animation
// clip. The animation clip is imported from the same file as the 3D model. Special import options are used to tell the
// importer to import data required for skeletal animation. It then proceeds to register the relevant keys used for
// controling the camera. Next it sets up the 3D scene using the mesh, textures, material and adds an animation
// component. The animation component start playing the animation clip we imported earlier. Finally it sets up a camera,
// along with CameraFlyer component that allows the user to fly around the scene.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// namespace bs {
// 	namespace ct {

namespace bs
{
	SPtr<ct::VertexBuffer> gInstanceBuffer;
	const UINT32 gNumInstances = 1000;

	UINT32 windowResWidth = 1280;
	UINT32 windowResHeight = 720;

	/** Container for all resources used by the example. */
	struct Assets
	{
		HMesh exampleModel;
		HAnimationClip exampleAnimClip;
		HTexture exampleAlbedoTex;
		HTexture exampleNormalsTex;
		HTexture exampleRoughnessTex;
		HTexture exampleMetalnessTex;
		HTexture exampleSkyCubemap;
		HMaterial exampleMaterial;
	};

	constexpr float FPS = 10;

	template<typename TCurveVector>
	void calcTimeRange(float& min, float& max, const TCurveVector& curves) {
		for (const auto& curve : curves) {
			std::pair<float, float> range = curve.curve.getTimeRange();
			min = std::min(min, range.first);
			max = std::max(max, range.second);
		}

	}

	std::pair<float, float> getTimeRange(SPtr<AnimationCurves> curves) {
		float start{100}, end{-100};
		calcTimeRange(start, end, curves->position);
		calcTimeRange(start, end, curves->rotation);
		calcTimeRange(start, end, curves->scale);
		calcTimeRange(start, end, curves->generic);
		return {start, end};
	}


	struct Data {
		Vector3 position;
		float frame;
		float frame1;
		float frame2;
		float frame3;
	};


	void setupInstancing(Assets& assets) {

		auto mesh = assets.exampleModel->getCore();
		SPtr<ct::VertexData> vertexData = mesh->getVertexData();

		SPtr<VertexDataDesc> vertexDesc = VertexDataDesc::create();
		*vertexDesc = *mesh->getVertexDesc();
		vertexDesc->addVertElem(
			 VET_FLOAT3, // Each entry in the instance vertex buffer is a 3D float
			 VES_POSITION, // We map it to the position semantic as vertex shader input
			 1, // We use the semantic index 1, as 0th index is taken by per-vertex VES_POSITION semantic
			 1, // We use the second bound vertex buffer for instance data
			 1  // Instance step rate of 1 means the new element will be fetched from the vertex buffer for each drawn instance
		 );
		vertexDesc->addVertElem(
			 VET_FLOAT4, // Each entry in the instance vertex buffer is a 3D float
			 VES_COLOR, // We map it to the position semantic as vertex shader input
			 1, // We use the semantic index 1, as 0th index is taken by per-vertex VES_POSITION semantic
			 1, // We use the second bound vertex buffer for instance data
			 1  // Instance step rate of 1 means the new element will be fetched from the vertex buffer for each drawn instance
		 );

		auto decl = ct::VertexDeclaration::create(vertexDesc);

		VERTEX_BUFFER_DESC vbDesc;
		vbDesc.vertexSize = decl->getProperties().getVertexSize(1);
		vbDesc.numVerts = gNumInstances;
		vbDesc.usage = GBU_STATIC;

		gInstanceBuffer = ct::VertexBuffer::create(vbDesc);


		Data data[gNumInstances];
		for (UINT32 i = 0; i < gNumInstances; ++i) {
			data[i].position = Vector3(i % 100, 0, i / 100);
			data[i].frame = 0;
		}
		assert(sizeof(Data) == vbDesc.vertexSize);
		gInstanceBuffer->writeData(0, sizeof(data), data, BWT_NORMAL);

		vertexData->vertexDeclaration = decl;
		vertexData->setBuffer(vertexData->getMaxBufferIndex() + 1, gInstanceBuffer);

	}

	void setBoneTransform(Color* colors, Matrix4& transform) {
		auto basis = transform.get3x3();
		// Vector3 position(transform[0][3], transform[1][3], transform[2][3]);
		colors[0][0] = transform[0][0] / 255.f;
		colors[0][1] = transform[0][1] / 255.f;
		colors[0][2] = transform[0][2] / 255.f;
		colors[0][3] = transform[0][3] / 255.f;
		colors[1][0] = transform[1][0] / 255.f;
		colors[1][1] = transform[1][1] / 255.f;
		colors[1][2] = transform[1][2] / 255.f;
		colors[1][3] = transform[1][3] / 255.f;
		colors[2][0] = transform[2][0] / 255.f;
		colors[2][1] = transform[2][1] / 255.f;
		colors[2][2] = transform[2][2] / 255.f;
		colors[2][3] = transform[2][3] / 255.f;

		assert(transform[3][0] == 0.f);
		assert(transform[3][1] == 0.f);
		assert(transform[3][2] == 0.f);
		assert(transform[3][3] == 1.f);
	}

	HTexture getSkeletonBoneTransforms(SPtr<Skeleton> skel, HAnimationClip clip) {

		UINT32 curBoneIdx = 0;
		UINT32 numBones = skel->getNumBones();
		Vector<Matrix4> transforms(numBones);

		LocalSkeletonPose localPose(numBones);
		// Copy transforms from mapped scene objects
		UINT32 boneTfrmIdx = 0;

		// std::pair<float, float> range = getTimeRange(clip->getCurves());
		// float start = range.first;
		// float end = range.second;
		// UINT32 frames = (end - start) * FPS;
		UINT32 frames = clip->getLength() * FPS;
		assert(frames > 1);
		// UINT32 frames = 1;
		// 3 pixels per bone transform.
		UINT32 width = numBones * 3;
		UINT32 height = frames;
		Vector<Color> colors(width * height);

		SkeletonMask mask(numBones);
		bool loop = true;
		float time = 0.f;
		// Animate bones
		// UINT32 frame = 0;
		std::cout << "NUMB BONES " << numBones << std::endl;
		std::cout << " NUM FRAMES " << frames << " " << clip->getLength() << " " << clip->getSampleRate() <<  std::endl;
		for (UINT32 frame = 0; frame < frames; ++frame) {
			// have to set hasOverride to false all manually.
			memset(localPose.hasOverride, 0, sizeof(bool) * localPose.numBones);
			float time = frame / FPS;
			skel->getPose(transforms.data(), localPose, mask, *clip, time, loop);

			for (UINT32 i = 0; i < transforms.size(); ++i) {
				// transforms[i] = Matrix4::TRS(localPose.positions[i], localPose.rotations[i], localPose.scales[i]);

				// Vector3 trans, scale;
				// Quaternion rot;
				// transforms[i].decomposition(trans, rot, scale);
				// Radian x, y, z;
				// rot.toEulerAngles(x,y,z);
				// std::cout << "TRANSFORM ROT ? " << i << " " << x.valueDegrees() << " " << y.valueDegrees() << " " << z.valueDegrees() << std::endl;

				assert(transforms[i].isAffine());

				UINT32 offset = i * 3;
				offset += width * frame;
				setBoneTransform(&colors[offset], transforms[i]);
				// std::cout << "OFFSET " << offset << " " << width << std::endl;
			}
		}

		UINT32 depth = 1;
		auto pixelData = PixelData::create(width, height, depth, PF_RGBA32F);
		pixelData->setColors(colors);

		HTexture texture = Texture::create(pixelData);
		return texture;
	}

	/** Load the resources we'll be using throughout the example. */
	Assets loadAssets()
	{
		Assets assets;

		// Load the 3D model and the animation clip

		// Set up a path to the model resource
		const Path exampleDataPath = EXAMPLE_DATA_PATH;
		const Path modelPath = exampleDataPath + "MechDrone/BaseMesh_Anim.fbx";

		// Set up mesh import options so that we import information about the skeleton and the skin, as well as any
		// animation clips the model might have.
		SPtr<MeshImportOptions> meshImportOptions = MeshImportOptions::create();
		meshImportOptions->importSkin = (true);
		meshImportOptions->importAnimation = (true);

		// The FBX file contains multiple resources (a mesh and an animation clip), therefore we use importAll() method,
		// which imports all resources in a file.
		SPtr<MultiResource> modelResources = gImporter().importAll(modelPath, meshImportOptions);
		for(auto& entry : modelResources->entries)
		{
			if(rtti_is_of_type<Mesh>(entry.value.get())) {
				assets.exampleModel = static_resource_cast<Mesh>(entry.value);
			}
			else if(rtti_is_of_type<AnimationClip>(entry.value.get())) {
				assets.exampleAnimClip = static_resource_cast<AnimationClip>(entry.value);
				std::cout << assets.exampleAnimClip->getName() << std::endl;			
			}
		}

		// Load PBR textures for the 3D model
		assets.exampleAlbedoTex = ExampleFramework::loadTexture(ExampleTexture::DroneAlbedo);
		assets.exampleNormalsTex = ExampleFramework::loadTexture(ExampleTexture::DroneNormal, false);
		assets.exampleRoughnessTex = ExampleFramework::loadTexture(ExampleTexture::DroneRoughness, false);
		assets.exampleMetalnessTex = ExampleFramework::loadTexture(ExampleTexture::DroneMetalness, false);

		// Create a material using the default physically based shader, and apply the PBR textures we just loaded
		// HShaderv shader = gBuiltinResources().getBuiltinShader(BuiltinShader::Standard);
		HShader shader = gImporter().import<Shader>(Path("/home/pgruenbacher/build/bsframework/bsfExamples/Build/Diffuse.bsl"));
		assets.exampleMaterial = Material::create(shader);

		assets.exampleMaterial->setTexture("gAlbedoTex", assets.exampleAlbedoTex);
		assets.exampleMaterial->setTexture("gNormalTex", assets.exampleNormalsTex);
		assets.exampleMaterial->setTexture("gRoughnessTex", assets.exampleRoughnessTex);
		assets.exampleMaterial->setTexture("gMetalnessTex", assets.exampleMetalnessTex);

		// Load an environment map
		assets.exampleSkyCubemap = ExampleFramework::loadTexture(ExampleTexture::EnvironmentRathaus, false, true, true);

		return assets;
	}

	/** Set up the 3D object used by the example, and the camera to view the world through. */
	void setUp3DScene(const Assets& assets)
	{
		/************************************************************************/
		/* 									RENDERABLE                  		*/
		/************************************************************************/

		// Now we create a scene object that has a position, orientation, scale and optionally components to govern its 
		// logic. In this particular case we are creating a SceneObject with a Renderable component which will render a
		// mesh at the position of the scene object with the provided material.

		// Create new scene object at (0, 0, 0)
		HSceneObject droneSO = SceneObject::create("Drone");
		
		// Attach the Renderable component and hook up the mesh we loaded, and the material we created.
		HRenderable renderable = droneSO->addComponent<CRenderable>();
		renderable->setMesh(assets.exampleModel);
		renderable->setMaterial(assets.exampleMaterial);

		/************************************************************************/
		/* 									ANIMATION	                  		*/
		/************************************************************************/

		// Add an animation component to the same scene object we added Renderable to.
		HAnimation animation = droneSO->addComponent<CAnimation>();

		// Start playing the animation clip we imported
		animation->play(assets.exampleAnimClip);

  		// gCoreThread().queueCommand(std::bind(getSkeletonBoneTransforms, assets.exampleModel->getSkeleton(), assets.exampleAnimClip));
  		HTexture texture = getSkeletonBoneTransforms(assets.exampleModel->getSkeleton(), assets.exampleAnimClip);
		assets.exampleMaterial->setTexture("gAnimationTex", texture);
		/************************************************************************/
		/* 									SKYBOX                       		*/
		/************************************************************************/

		// Add a skybox texture for sky reflections
		HSceneObject skyboxSO = SceneObject::create("Skybox");

		HSkybox skybox = skyboxSO->addComponent<CSkybox>();
		skybox->setTexture(assets.exampleSkyCubemap);

		/************************************************************************/
		/* 									CAMERA	                     		*/
		/************************************************************************/

		// In order something to render on screen we need at least one camera.

		// Like before, we create a new scene object at (0, 0, 0).
		HSceneObject sceneCameraSO = SceneObject::create("SceneCamera");

		// Get the primary render window we need for creating the camera. 
		SPtr<RenderWindow> window = gApplication().getPrimaryWindow();

		// Add a Camera component that will output whatever it sees into that window 
		// (You could also use a render texture or another window you created).
		HCamera sceneCamera = sceneCameraSO->addComponent<CCamera>();
		sceneCamera->getViewport()->setTarget(window);

		// Set up camera component properties

		// Set closest distance that is visible. Anything below that is clipped.
		sceneCamera->setNearClipDistance(0.005f);

		// Set farthest distance that is visible. Anything above that is clipped.
		sceneCamera->setFarClipDistance(1000);

		// Set aspect ratio depending on the current resolution
		sceneCamera->setAspectRatio(windowResWidth / (float)windowResHeight);

		// Enable indirect lighting so we get accurate diffuse lighting from the skybox environment map
		const SPtr<RenderSettings>& renderSettings = sceneCamera->getRenderSettings();
		renderSettings->enableIndirectLighting = true;

		sceneCamera->setRenderSettings(renderSettings);

		// Add a CameraFlyer component that allows us to move the camera. See CameraFlyer for more information.
		sceneCameraSO->addComponent<CameraFlyer>();

		// Position and orient the camera scene object
		sceneCameraSO->setPosition(Vector3(0.0f, 2.5f, -4.0f) * 0.65f);
		sceneCameraSO->lookAt(Vector3(0, 1.5f, 0));
	}

	class AnimApplication : public Application {
		Data mData[gNumInstances];
	public:
		AnimApplication(START_UP_DESC d) : Application(d) {

			for (UINT32 i = 0; i < gNumInstances; ++i) {
				mData[i].position = Vector3(i % 100, 0, i / 100);
				UINT32 frameOffset = (i * 7) / 40;
				mData[i].frame = (frameOffset) % 14;
			}
		}

		void updateInstancing(float time) {

			for (UINT32 i = 0; i < gNumInstances; ++i) {
				mData[i].frame += time * FPS;
				if (mData[i].frame >= 16.f) {
					mData[i].frame = mData[i].frame - 16.f;
				}
				// mData[i].frame = 15.f;
				if (i == 0) std::cout << "FRAME ? " << mData[i].frame << std::endl;
			}
			gInstanceBuffer->writeData(0, sizeof(mData), mData, BWT_NORMAL);
		}

		void preUpdate() override {
			Application::preUpdate();
			gCoreThread().queueCommand(std::bind(&AnimApplication::updateInstancing, this, gTime().getFrameDelta()));
		}
	};
} // namespace bs



/** Main entry point into the application. */
#if BS_PLATFORM == BS_PLATFORM_WIN32
#include <windows.h>

int CALLBACK WinMain(
	_In_  HINSTANCE hInstance,
	_In_  HINSTANCE hPrevInstance,
	_In_  LPSTR lpCmdLine,
	_In_  int nCmdShow
	)
#else
int main()
#endif
{
	using namespace bs;

	// Initializes the application and creates a window with the specified properties
	VideoMode videoMode(windowResWidth, windowResHeight);
	Application::startUp<AnimApplication>(videoMode, "Example", false);

	// Registers a default set of input controls
	ExampleFramework::setupInputConfig();

	// Load a model and textures, create materials
	Assets assets = loadAssets();

	gCoreThread().queueCommand(std::bind(setupInstancing, assets));
	// Set up the scene with an object to render and a camera
	setUp3DScene(assets);
	
	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::instance().runMainLoop();

	gInstanceBuffer.reset();
	// When done, clean up
	Application::shutDown();


	return 0;
}
