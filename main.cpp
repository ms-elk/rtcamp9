#include <alpine.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <chrono>
#include <thread>

// https://github.com/richgel999/fpng
#include <fpng.h>

static constexpr float PI = 3.14159265358979323846f;

int32_t main(int32_t argc, const char* argv[])
{
    // レンダラー起動時間を取得。
    using clock = std::chrono::high_resolution_clock;
    const clock::time_point appStartTp = clock::now();

    uint32_t startFrameIndex = 0;
    uint32_t endFrameIndex = 0;
    std::string_view gltfName = "";
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t spp = 0;
    uint32_t fps = 0;
    for (int argIdx = 1; argIdx < argc; ++argIdx)
    {
        std::string_view arg = argv[argIdx];
        if (arg == "--frame-range")
        {
            if (argIdx + 2 >= argc)
            {
                printf("--frame-range requires a frame number pair to start and end.\n");
                return -1;
            }
            startFrameIndex = static_cast<uint32_t>(atoi(argv[argIdx + 1]));
            endFrameIndex = static_cast<uint32_t>(atoi(argv[argIdx + 2]));
            argIdx += 2;
        }
        else if (arg == "--gltf")
        {
            if (argIdx + 1 >= argc)
            {
                return -1;
            }
            gltfName = argv[argIdx + 1];
            argIdx++;
        }
        else if (arg == "--width")
        {
            if (argIdx + 1 >= argc)
            {
                return -1;
            }
            width = static_cast<uint32_t>(atoi(argv[argIdx + 1]));
            argIdx++;
        }
        else if (arg == "--height")
        {
            if (argIdx + 1 >= argc)
            {
                return -1;
            }
            height = static_cast<uint32_t>(atoi(argv[argIdx + 1]));
            argIdx++;
        }
        else if (arg == "--spp")
        {
            if (argIdx + 1 >= argc)
            {
                return -1;
            }
            spp = static_cast<uint32_t>(atoi(argv[argIdx + 1]));
            argIdx++;
        }
        else if (arg == "--fps")
        {
            if (argIdx + 1 >= argc)
            {
                return -1;
            }
            fps = static_cast<uint32_t>(atoi(argv[argIdx + 1]));
            argIdx++;
        }
        else
        {
            printf("Unknown argument %s.\n", argv[argIdx]);
            return -1;
        }
    }

    if (endFrameIndex < startFrameIndex)
    {
        printf("Invalid frame range.\n");
        return -1;
    }

    const uint32_t maxDepth = 8;

    alpine::initialize(width, height, maxDepth);
    alpine::setBackgroundColor(0.0f, 0.0f, 0.0f);

    const float deltaLightRot = PI / 9.0f;
    std::array<alpine::ILight*, 2> lightGroup0;
    {
        float emisssion[] = { 10.0f, 10.0f, 10.0f };
        float lightPos[] = { 0.0f, 2.9f, 0.0f };
        float lightRadius = 1.0f;
        lightGroup0[0] = alpine::addDiskLight(emisssion, lightPos, lightRadius);
    }

    {
        float emisssion[] = { 30.0f, 20.0f, 5.0f };
        float lightPos[] = { 0.0f, 2.5f, -4.0f };
        float lightRadius = 0.3f;
        lightGroup0[1] = alpine::addDiskLight(emisssion, lightPos, lightRadius);
    }

    std::array<alpine::ILight*, 3> lightGroup1;
    {
        float emisssion[] = { 0.0f, 50.0f, 50.0f };
        float lightPos[] = { 1.5f, 2.8f, 1.5f };
        lightGroup1[0] = alpine::addDiskLight(emisssion, lightPos, 0.3f);
        lightGroup1[0]->enable(false);
    }

    {
        float emisssion[] = { 50.0f, 0.0f, 50.0f };
        float lightPos[] = { 1.5f, 2.8f, -1.5f };
        lightGroup1[1] = alpine::addDiskLight(emisssion, lightPos, 0.3f);
        lightGroup1[1]->enable(false);
    }

    {
        float emisssion[] = { 50.0f, 50.0f, 0.0f };
        float lightPos[] = { -1.5f, 2.8f, 1.5f };
        lightGroup1[2] = alpine::addDiskLight(emisssion, lightPos, 0.3f);
        lightGroup1[2]->enable(false);
    }

    const float eye0[] = { 2.24f, 1.8f, -6.64f };
    const float target0[] = { 3.0f, 0.8f, 0.0f };
    const float up[] = { 0.0f, 1.0f, 0.0f };

    const float eye1[] = { 2.5f, 1.8f, -1.6f };
    const float target1[] = { 0.18f, 0.8f, 0.2f };

    const float fovy = PI / 2.0f;
    const float aspect = float(width) / float(height);

    auto* camera = alpine::getCamera();
    camera->setLookAt(eye0, target0, up, fovy, aspect);

    bool loaded = alpine::load(gltfName, alpine::FileType::GLTF);
    if (!loaded)
    {
        printf("ERROR: failed to load %s\n", gltfName.data());
        return 1;
    }

    fpng::fpng_init();

    uint32_t lightOffFrame = 3 * fps;
    uint32_t lightAnimFrame = 4 * fps;

    for (uint32_t frameIndex = startFrameIndex; frameIndex <= endFrameIndex; ++frameIndex)
    {
        const clock::time_point frameStartTp = clock::now();
        printf("Frame %u ... ", frameIndex);

        if (frameIndex >= lightAnimFrame) // Use Light Group 1
        {
            lightGroup0[0]->enable(false);
            lightGroup0[1]->enable(false);

            float scale = static_cast<float>(frameIndex - lightAnimFrame) / (0.3f * static_cast<float>(fps));
            scale = std::min(scale, 1.0f);

            for (uint32_t i = 0; i < lightGroup1.size(); ++i)
            {
                float offset = static_cast<float>(i)* 2.0f * PI / static_cast<float>(lightGroup1.size());
                float step = static_cast<float>(frameIndex - lightAnimFrame) / (static_cast<float>(fps) / 10.0f);
                float theta = step * deltaLightRot + offset;
                float p[] = { cosf(theta), 2.8f, sinf(theta) };

                auto& l1 = lightGroup1[i];
                l1->setPosition(p);
                l1->enable(true);
                l1->setScale(scale);
            }
        }
        else // Use Light Group 0
        {
            // Light fade out
            float scale = frameIndex >= lightOffFrame ?
                1.0f - static_cast<float>(frameIndex - lightOffFrame) / static_cast<float>(lightAnimFrame - lightOffFrame - 1) : 1.0f;
 
            for (auto& l0 : lightGroup0)
            {
                l0->enable(true);
                l0->setScale(scale);
            }

            for (auto& l1 : lightGroup1)
            {
                l1->enable(false);
            }
        }

        float t = static_cast<float>(frameIndex) / static_cast<float>(lightOffFrame - 1);
        t = std::min(t, 1.0f);

        float eye[3];
        eye[0] = eye0[0] * (1.0f - t) + eye1[0] * t;
        eye[1] = eye0[1] * (1.0f - t) + eye1[1] * t;
        eye[2] = eye0[2] * (1.0f - t) + eye1[2] * t;

        float target[3];
        target[0] = target0[0] * (1.0f - t) + target1[0] * t;
        target[1] = target0[1] * (1.0f - t) + target1[1] * t;
        target[2] = target0[2] * (1.0f - t) + target1[2] * t;

        camera->setLookAt(eye, target, up, fovy, aspect);

        alpine::resetAccumulation();
        alpine::render(spp);
        alpine::resolve(true);

        // 起動からの時刻とフレーム時間を計算。
        const clock::time_point now = clock::now();
        const clock::duration frameTime = now - frameStartTp;
        const clock::duration totalTime = now - appStartTp;
        printf(
            "Done: %.3f [ms] (total: %.3f [s])\n",
            std::chrono::duration_cast<std::chrono::microseconds>(frameTime).count() * 1e-3f,
            std::chrono::duration_cast<std::chrono::milliseconds>(totalTime).count() * 1e-3f);

        // 3桁連番で画像出力。
        char filename[256];
        sprintf_s(filename, "%03u.png", frameIndex);
        const void* pixels = alpine::getFrameBuffer();
        fpng::fpng_encode_image_to_file(filename, pixels, width, height, 3, 0);
    }

    return 0;
}
