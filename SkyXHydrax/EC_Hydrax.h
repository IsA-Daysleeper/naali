/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_Hydrax.h
 *  @brief  A photorealistic water plane component using Hydrax, http://www.ogre3d.org/tikiwiki/Hydrax
 */

#pragma once

#include "IComponent.h"
#include "Vector3D.h"

struct EC_HydraxImpl;

/// A photorealistic water plane component using Hydrax, http://www.ogre3d.org/tikiwiki/Hydrax
/** @note Requires Hydrax Ogre add-on. */
class EC_Hydrax : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("EC_Hydrax", 777)

public:
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_Hydrax(Scene* scene);
    ~EC_Hydrax();

    /// 
    DEFINE_QPROPERTY_ATTRIBUTE(Vector3df, position);
    Q_PROPERTY(Vector3df position READ getposition WRITE setposition);

private:
    EC_HydraxImpl *impl;

private slots:
    void UpdateAttribute(IAttribute *attr);
    void Update(float frameTime);
};
