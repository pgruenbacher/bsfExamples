#pragma once

#include "BsPrerequisites.h"
#include "Reflection/BsRTTIType.h"
#include "Resources/BsResources.h"
#include "Mesh/BsMesh.h"
#include "Importer/BsImporter.h"
#include "Importer/BsMeshImportOptions.h"
#include "Importer/BsTextureImportOptions.h"
#include "BsExampleConfig.h"

namespace bs
{
	/** A list of mesh assets provided with the example projects. */
	enum class ExampleMesh
	{
		Pistol
	};

	/** A list of texture assets provided with the example projects. */
	enum class ExampleTexture
	{
		PistolAlbedo,
		PistolNormal,
		PistolRoughness,
		PistolMetalness,
		EnvironmentPaperMill
	};

	/** A list of shader assets provided with the example projects. */
	enum class ExampleShader
	{
		CustomVertex,
		CustomDeferredSurface,
		CustomDeferredLighting,
		CustomForward
	};

	/** Various helper functionality used throught the examples. */
	class ExampleFramework
	{
	public:
		/** Registers a common set of keys/buttons that are used for controlling the examples. */
		static void setupInputConfig()
		{
			// Register input configuration
			// bsf allows you to use VirtualInput system which will map input device buttons and axes to arbitrary names,
			// which allows you to change input buttons without affecting the code that uses it, since the code is only
			// aware of the virtual names.  If you want more direct input, see Input class.
			auto inputConfig = gVirtualInput().getConfiguration();

			// Camera controls for buttons (digital 0-1 input, e.g. keyboard or gamepad button)
			inputConfig->registerButton("Forward", BC_W);
			inputConfig->registerButton("Back", BC_S);
			inputConfig->registerButton("Left", BC_A);
			inputConfig->registerButton("Right", BC_D);
			inputConfig->registerButton("Forward", BC_UP);
			inputConfig->registerButton("Back", BC_DOWN);
			inputConfig->registerButton("Left", BC_LEFT);
			inputConfig->registerButton("Right", BC_RIGHT);
			inputConfig->registerButton("FastMove", BC_LSHIFT);
			inputConfig->registerButton("RotateObj", BC_MOUSE_LEFT);
			inputConfig->registerButton("RotateCam", BC_MOUSE_RIGHT);

			// Camera controls for axes (analog input, e.g. mouse or gamepad thumbstick)
			// These return values in [-1.0, 1.0] range.
			inputConfig->registerAxis("Horizontal", VIRTUAL_AXIS_DESC((UINT32)InputAxis::MouseX));
			inputConfig->registerAxis("Vertical", VIRTUAL_AXIS_DESC((UINT32)InputAxis::MouseY));
		}

		/** 
		 * Loads one of the builtin mesh assets. If the asset doesn't exist, the mesh will be re-imported from the source
		 * file, and then saved so it can be loaded on the next call to this method. 
		 * 
		 * Use the 'scale' parameter to control the size of the mesh. Note this option is only relevant when a mesh is
		 * being imported (i.e. when the asset file is missing).
		 */
		static HMesh loadMesh(ExampleMesh type, float scale = 1.0f)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] =
			{
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol01.fbx",
			};

			const Path& srcAssetPath = assetPaths[(UINT32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.setExtension(srcAssetPath.getExtension() + ".asset");

			HMesh model = gResources().load<Mesh>(assetPath);
			if (model == nullptr) // Mesh file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<ImportOptions> meshImportOptions = Importer::instance().createImportOptions(srcAssetPath);

				// rtti_is_of_type checks if the import options are of valid type, in case the provided path is pointing to a
				// non-mesh resource. This is similar to dynamic_cast but uses Banshee internal RTTI system for type checking.
				if (rtti_is_of_type<MeshImportOptions>(meshImportOptions))
				{
					MeshImportOptions* importOptions = static_cast<MeshImportOptions*>(meshImportOptions.get());

					importOptions->setImportScale(scale);
				}

				model = gImporter().import<Mesh>(srcAssetPath, meshImportOptions);

				// Save for later use, so we don't have to import on the next run.
				gResources().save(model, assetPath, true);
			}

			return model;
		}

		/**
		 * Loads one of the builtin texture assets. If the asset doesn't exist, the texture will be re-imported from the 
		 * source file, and then saved so it can be loaded on the next call to this method. 
		 * 
		 * Textures not in sRGB space (e.g. normal maps) need to be specially marked by setting 'isSRGB' to false. Also 
		 * allows for conversion of texture to cubemap by setting the 'isCubemap' parameter. If the data should be imported
		 * in a floating point format, specify 'isHDR' to true. Note these options are only relevant when a texture is
		 * being imported (i.e. when asset file is missing).
		 */
		static HTexture loadTexture(ExampleTexture type, bool isSRGB = true, bool isCubemap = false, bool isHDR = false)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] =
			{
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_DFS.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_NM.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_RGH.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_MTL.png",
				Path(EXAMPLE_DATA_PATH) + "Environments/PaperMill_E_3k.hdr",
			};

			const Path& srcAssetPath = assetPaths[(UINT32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.setExtension(srcAssetPath.getExtension() + ".asset");

			HTexture texture = gResources().load<Texture>(assetPath);
			if (texture == nullptr) // Texture file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<ImportOptions> textureImportOptions = Importer::instance().createImportOptions(srcAssetPath);

				// rtti_is_of_type checks if the import options are of valid type, in case the provided path is pointing to a 
				// non-texture resource. This is similar to dynamic_cast but uses Banshee internal RTTI system for type checking.
				if (rtti_is_of_type<TextureImportOptions>(textureImportOptions))
				{
					TextureImportOptions* importOptions = static_cast<TextureImportOptions*>(textureImportOptions.get());

					// We want maximum number of mipmaps to be generated
					importOptions->setGenerateMipmaps(true);

					// If the texture is in sRGB space the system needs to know about it
					importOptions->setSRGB(isSRGB);

					// Ensures we can save the texture contents
					importOptions->setCPUCached(true);

					// Import as cubemap if needed
					importOptions->setIsCubemap(isCubemap);

					// If importing as cubemap, assume source is a panorama
					importOptions->setCubemapSourceType(CubemapSourceType::Cylindrical);

					// Importing using a HDR format if requested
					if (isHDR)
						importOptions->setFormat(PF_RG11B10F);
				}

				// Import texture with specified import options
				texture = gImporter().import<Texture>(srcAssetPath, textureImportOptions);

				// Save for later use, so we don't have to import on the next run.
				gResources().save(texture, assetPath, true);
			}

			return texture;
		}

		/** 
		 * Loads one of the builtin shader assets. If the asset doesn't exist, the shader will be re-imported from the 
		 * source file, and then saved so it can be loaded on the next call to this method. 
		 */
		static HShader loadShader(ExampleShader type)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] =
			{
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomVertex.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomDeferredSurface.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomDeferredLighting.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomForward.bsl",
			};

			const Path& srcAssetPath = assetPaths[(UINT32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.setExtension(srcAssetPath.getExtension() + ".asset");

			HShader shader = gResources().load<Shader>(assetPath);
			if (shader == nullptr) // Shader file doesn't exist, import from the source file.
			{
				shader = gImporter().import<Shader>(srcAssetPath);

				// Save for later use, so we don't have to import on the next run.
				gResources().save(shader, assetPath, true);
			}

			return shader;
		}
	};
}
