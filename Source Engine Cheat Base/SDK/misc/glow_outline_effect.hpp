#pragma once

#include "UtlVector.hpp"
#include "../Interfaces/IClientEntity.hpp"

class CGlowObjectManager {
public:

    int RegisterGlowObject(entity_t* pEntity, const Vector& vGlowColor, float flGlowAlpha, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded, int nSplitScreenSlot) {
        int nIndex;
        if (m_nFirstFreeSlot == GlowObjectDefinition_t::END_OF_FREE_LIST) {
            nIndex = m_GlowObjectDefinitions.AddToTail();
        }
        else {
            nIndex = m_nFirstFreeSlot;
            m_nFirstFreeSlot = m_GlowObjectDefinitions[nIndex].m_nNextFreeSlot;
        }

        m_GlowObjectDefinitions[nIndex].m_hEntity = pEntity;
        m_GlowObjectDefinitions[nIndex].m_vGlowColor = vGlowColor;
        m_GlowObjectDefinitions[nIndex].m_flGlowAlpha = flGlowAlpha;
        //m_GlowObjectDefinitions[nIndex].flUnk = 0.0f;
        m_GlowObjectDefinitions[nIndex].m_flBloomAmount = 1.0f;
        //m_GlowObjectDefinitions[nIndex].localplayeriszeropoint3 = 0.0f;
        m_GlowObjectDefinitions[nIndex].m_bRenderWhenOccluded = bRenderWhenOccluded;
        m_GlowObjectDefinitions[nIndex].m_bRenderWhenUnoccluded = bRenderWhenUnoccluded;
        m_GlowObjectDefinitions[nIndex].m_bFullBloomRender = false;
        //m_GlowObjectDefinitions[nIndex].m_nFullBloomStencilTestValue = 0;
        m_GlowObjectDefinitions[nIndex].m_nSplitScreenSlot = nSplitScreenSlot;
        m_GlowObjectDefinitions[nIndex].m_nNextFreeSlot = GlowObjectDefinition_t::ENTRY_IN_USE;

        return nIndex;
    }

    void UnregisterGlowObject(int nGlowObjectHandle) {
        Assert(!m_GlowObjectDefinitions[nGlowObjectHandle].IsUnused());

        m_GlowObjectDefinitions[nGlowObjectHandle].m_nNextFreeSlot = m_nFirstFreeSlot;
        m_GlowObjectDefinitions[nGlowObjectHandle].m_hEntity = NULL;
        m_nFirstFreeSlot = nGlowObjectHandle;
    }

    int HasGlowEffect(entity_t* pEntity) const { //-V2009
        for (int i = 0; i < m_GlowObjectDefinitions.Count(); ++i) {
            if (!m_GlowObjectDefinitions[i].IsUnused() && m_GlowObjectDefinitions[i].m_hEntity == pEntity) {
                return i;
            }
        }

        return NULL;
    }

    class GlowObjectDefinition_t {
    public:
        void Set(float r, float g, float b, float a, float bloom, int style) {
            m_vGlowColor = Vector(r, g, b);
            m_flGlowAlpha = a;
            m_bRenderWhenOccluded = true;
            m_bRenderWhenUnoccluded = false;
            m_flBloomAmount = bloom;
            m_nGlowStyle = style;
        }

        entity_t* GetEnt() {
            return m_hEntity;
        }

        bool IsUnused() const { return m_nNextFreeSlot != GlowObjectDefinition_t::ENTRY_IN_USE; }

    public:
        int m_nNextFreeSlot;
        entity_t* m_hEntity;

        Vector m_vGlowColor;
        float m_flGlowAlpha;

        bool m_bGlowAlphaCappedByRenderAlpha;
        float m_flGlowAlphaFunctionOfMaxVelocity;

        float m_flGlowAlphaMax;
        float m_flGlowPulseOverdrive;

        bool m_bRenderWhenOccluded;
        bool m_bRenderWhenUnoccluded;
        bool m_bFullBloomRender;
        int m_flBloomAmount; // m_nFullBloomStencilTestValue ?

        int m_nGlowStyle;
        int m_nSplitScreenSlot;

        static const int END_OF_FREE_LIST = -1;
        static const int ENTRY_IN_USE = -2;
    };

    /*struct GlowBoxDefinition_t
    {
        Vector m_vPosition;
        QAngle m_angOrientation;
        Vector m_vMins;
        Vector m_vMaxs;
        float m_flBirthTimeIndex;
        float m_flTerminationTimeIndex; //when to die
        Color m_colColor;
    };
    CUtlVector< GlowBoxDefinition_t > m_GlowBoxDefinitions;*/

    CUtlVector< GlowObjectDefinition_t > m_GlowObjectDefinitions;

    int m_nFirstFreeSlot;

};