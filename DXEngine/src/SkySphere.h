#pragma once
#include "utils/mesh\Mesh.h"
#include "dxpch.h"
#include <memory>
#include <models/Model.h>


namespace DXEngine {

    class SkySphere : public Model
    {
    public:
        SkySphere();

    private:
        void Initialize();
    };

}