// Copyright Epic Games, Inc. All Rights Reserved.


#include "Parameters.h"
#include "Platform.h"

#include "ParametersPrivate.h"

#include "ModelPrivate.h"
#include "Operations.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	Parameters::Parameters()
	{
		m_pD = new Private();
	}


	//---------------------------------------------------------------------------------------------
	Parameters::~Parameters()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	void Parameters::Serialise( const Parameters* p, OutputArchive& arch )
	{
		arch << *p->m_pD;
    }


	//---------------------------------------------------------------------------------------------
	ParametersPtr Parameters::StaticUnserialise( InputArchive& arch )
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		ParametersPtr pResult = new Parameters();
		arch >> *pResult->m_pD;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void Parameters::SetUnserialisedModel( Model* pModel )
	{
		m_pD->m_pModel = pModel;
	}


    //---------------------------------------------------------------------------------------------
    void Parameters::SerialisePortable( const Parameters* params, OutputArchive& arch )
    {
        const int32_t ver = 0;
        arch << ver;

        uint32_t valueCount = uint32_t(params->GetPrivate()->m_values.size());
        arch << valueCount;

        for (size_t p=0; p<valueCount; ++p)
        {
            const auto& desc = params->GetPrivate()->m_pModel->GetPrivate()->m_program.m_parameters[p];
            arch << desc.m_name;
            arch << desc.m_uid;
            arch << desc.m_type;
            arch << params->GetPrivate()->m_values[p];
        }
    }


    //---------------------------------------------------------------------------------------------
    ParametersPtr Parameters::UnserialisePortable( InputArchive& arch, const Model* pModel )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        ParametersPtr pResult = pModel->NewParameters();
        size_t modelParameters = pModel->GetPrivate()->m_program.m_parameters.size();

        int32_t ver;
        arch >> ver;
        check( ver == 0 );

        uint32_t valueCount = 0;
        arch >> valueCount;

        for (uint32_t p=0; p<valueCount; ++p)
        {
            // Read the parameter data.
            mu::string name;
            arch >> name;

            mu::string uid;
            arch >> uid;

            PARAMETER_TYPE type;
            arch >> type;

            PARAMETER_VALUE value;
            arch >> value;

            int modelParam = -1;

            // UIDs have higher priority
            if ( !uid.empty() )
            {
                for (size_t mp=0; mp<modelParameters; ++mp)
                {
                    const auto& desc = pModel->GetPrivate()->m_program.m_parameters[mp];
                    if (desc.m_uid==uid)
                    {
                        modelParam = int(mp);
                        break;
                    }
                }
            }

            // Try with name+type
            if (modelParam<0)
            {
                for (size_t mp=0; mp<modelParameters; ++mp)
                {
                    const auto& desc = pModel->GetPrivate()->m_program.m_parameters[mp];
                    if (desc.m_name==name && desc.m_type==type)
                    {
                        modelParam = int(mp);
                        break;
                    }
                }
            }

            // Try with name only, and maybe adapt type
            if (modelParam<0)
            {
                for (size_t mp=0; mp<modelParameters; ++mp)
                {
                    const auto& desc = pModel->GetPrivate()->m_program.m_parameters[mp];
                    if (desc.m_name==name)
                    {
                        modelParam = int(mp);
                        break;
                    }
                }
            }

            if (modelParam>=0)
            {
                // We found something. \todo: convert the type if necessary?
                pResult->GetPrivate()->m_values[modelParam] = value;
            }
        }

        return pResult;
    }


	//---------------------------------------------------------------------------------------------
	Parameters::Private* Parameters::GetPrivate() const
	{
		return m_pD;
	}


	//---------------------------------------------------------------------------------------------
	ParametersPtr Parameters::Clone() const
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		ParametersPtr pRes = new Parameters();

		pRes->m_pD->m_pModel = m_pD->m_pModel;
		pRes->m_pD->m_values = m_pD->m_values;
		pRes->m_pD->m_multiValues = m_pD->m_multiValues;

		return pRes;
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::GetCount() const
	{
		return (int)m_pD->m_values.size();
	}


	//---------------------------------------------------------------------------------------------
	const char* Parameters::GetName( int index ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( index>=0 && index<(int)program.m_parameters.size() );

		return program.m_parameters[index].m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	const char* Parameters::GetUid( int index ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( index>=0 && index<(int)program.m_parameters.size() );

		return program.m_parameters[index].m_uid.c_str();
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::Find( const char* strName ) const
	{
		return m_pD->Find( strName );
	}


	//---------------------------------------------------------------------------------------------
	PARAMETER_TYPE Parameters::GetType( int index ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( index>=0 && index<(int)program.m_parameters.size() );

		return program.m_parameters[index].m_type;
	}


	//---------------------------------------------------------------------------------------------
	PARAMETER_DETAILED_TYPE Parameters::GetDetailedType( int index ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( index>=0 && index<(int)program.m_parameters.size() );

		return program.m_parameters[index].m_detailedType;
	}


    //---------------------------------------------------------------------------------------------
    Ptr<RangeIndex> Parameters::NewRangeIndex( int paramIndex ) const
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
        check( paramIndex>=0 && paramIndex<int(program.m_parameters.size()) );
        Ptr<RangeIndex> range;

        if ( paramIndex>=0 && paramIndex<int(program.m_parameters.size()) )
        {
            size_t rangeCount = program.m_parameters[paramIndex].m_ranges.size();
            if ( rangeCount>0 )
            {
                range = new RangeIndex;
                range->m_pD->m_pParameters = this;
                range->m_pD->m_parameter = paramIndex;
                range->m_pD->m_values.resize( rangeCount, 0 );
            }
        }

        return range;
    }


    //---------------------------------------------------------------------------------------------
    int Parameters::GetValueCount( int paramIndex ) const
    {
        if (paramIndex<0 || paramIndex >= int(m_pD->m_multiValues.size()) )
        {
            return 0;
        }

        return int( m_pD->m_multiValues[paramIndex].size() );
    }


    //---------------------------------------------------------------------------------------------
    Ptr<RangeIndex> Parameters::GetValueIndex( int paramIndex, int valueIndex ) const
    {
        if (paramIndex<0 || paramIndex >= int(m_pD->m_multiValues.size()) )
        {
            return nullptr;
        }

        if (valueIndex<0 || valueIndex >= int(m_pD->m_multiValues[paramIndex].size()) )
        {
            return nullptr;
        }

        auto it = m_pD->m_multiValues[paramIndex].begin();
        for ( int i=0; i<valueIndex; ++i )
        {
            ++it;
        }

        Ptr<RangeIndex> result = NewRangeIndex( paramIndex );
        result->m_pD->m_values = it->first;

        return result;
    }


    //---------------------------------------------------------------------------------------------
    void Parameters::ClearAllValues( int paramIndex )
    {
        if (paramIndex<0 || paramIndex >= int(m_pD->m_multiValues.size()) )
        {
            return;
        }

        m_pD->m_multiValues[paramIndex].clear();
    }


	//---------------------------------------------------------------------------------------------
	int Parameters::GetAdditionalImageCount( int index ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( index>=0 && index<(int)program.m_parameters.size() );
		return (int)program.m_parameters[index].m_descImages.size();
	}


	//---------------------------------------------------------------------------------------------
    bool Parameters::GetBoolValue( int index,
                                   const Ptr<const RangeIndex>& pos ) const
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
        check( GetType(index)==PARAMETER_TYPE::T_BOOL );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_BOOL )
        {
            return false;
        }

        // Single value case
        if (!pos)
        {
            // Return the single value
            return m_pD->m_values[index].m_bool;
        }

        // Multivalue case
        check( pos->m_pD->m_parameter==index );

        if ( index<int(m_pD->m_multiValues.size()))
        {
            const auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                return it->second.m_bool;
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        return m_pD->m_values[index].m_bool;
	}


	//---------------------------------------------------------------------------------------------
    void Parameters::SetBoolValue( int index, bool value,
                                   const Ptr<const RangeIndex>& pos )
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_BOOL );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_BOOL )
        {
            return;
        }

        // Single value case
        if (!pos)
        {
            // Clear multivalue, if set.
            if (index<int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues[index].clear();
            }

            m_pD->m_values[index].m_bool = value;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter==index );

            if ( index>=int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues.resize(index+1);
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                it->second.m_bool = value;
            }
            else
            {
                PARAMETER_VALUE valueData;
                valueData.m_bool = value;
                m.insert( std::make_pair<>( pos->m_pD->m_values, valueData ) );
            }
        }
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::GetIntPossibleValueCount( int paramIndex ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( paramIndex>=0 && paramIndex<(int)program.m_parameters.size() );
		return (int)program.m_parameters[paramIndex].m_possibleValues.size();
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::GetIntPossibleValue( int paramIndex, int valueIndex ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( paramIndex>=0
				&& paramIndex<(int)program.m_parameters.size() );
		check( valueIndex>=0
				&& valueIndex<(int)program.m_parameters[paramIndex].m_possibleValues.size() );

		return (int)program.m_parameters[paramIndex].m_possibleValues[valueIndex].m_value;
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::GetIntValueIndex(int paramIndex, const char* valueName) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check(paramIndex >= 0
			&& paramIndex < (int)program.m_parameters.size());

		int result = -1;
		for (size_t v = 0; v < program.m_parameters[paramIndex].m_possibleValues.size(); ++v)
		{
			if (program.m_parameters[paramIndex].m_possibleValues[v].m_name == valueName)
			{
				result = (int)v;
				break;
			}
		}
		return result;
	}


	//---------------------------------------------------------------------------------------------
	int Parameters::GetIntValueIndex(int paramIndex, int32 Value) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check(paramIndex >= 0
			&& paramIndex < (int)program.m_parameters.size());

		for (size_t v = 0; v < program.m_parameters[paramIndex].m_possibleValues.size(); ++v)
		{
			if (program.m_parameters[paramIndex].m_possibleValues[v].m_value == Value)
			{
				return (int)v;
			}
		}
		return -1;
	}


	//---------------------------------------------------------------------------------------------
	const char* Parameters::GetIntPossibleValueName( int paramIndex, int valueIndex ) const
	{
		const PROGRAM& program = m_pD->m_pModel->GetPrivate()->m_program;
		check( paramIndex>=0
				&& paramIndex<(int)program.m_parameters.size() );
		check( valueIndex>=0
				&& valueIndex<(int)program.m_parameters[paramIndex].m_possibleValues.size() );

		return program.m_parameters[paramIndex].m_possibleValues[valueIndex].m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
    int Parameters::GetIntValue( int index,
                                 const Ptr<const RangeIndex>& pos ) const
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_INT );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_INT )
        {
            return 0;
        }

        // Single value case
        if (!pos)
        {
            // Return the single value
            return m_pD->m_values[index].m_int;
        }

        // Multivalue case
        check( pos->m_pD->m_parameter==index );

        if ( index<int(m_pD->m_multiValues.size()))
        {
            const auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                return it->second.m_int;
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        return m_pD->m_values[index].m_int;
    }


	//---------------------------------------------------------------------------------------------
    void Parameters::SetIntValue( int index, int value,
                                  const Ptr<const RangeIndex>& pos )
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_INT );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_INT )
        {
            return;
        }

        // Single value case
        if (!pos)
        {
            // Clear multivalue, if set.
            if (index<int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues[index].clear();
            }

            m_pD->m_values[index].m_int = value;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter==index );

            if ( index>=int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues.resize(index+1);
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                it->second.m_int = value;
            }
            else
            {
                PARAMETER_VALUE valueData;
                valueData.m_int = value;
                m.insert( std::make_pair<>( pos->m_pD->m_values, valueData ) );
            }
        }
    }


	//---------------------------------------------------------------------------------------------
    float Parameters::GetFloatValue( int index,
                                     const Ptr<const RangeIndex>& pos ) const
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_FLOAT );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_FLOAT )
        {
            return false;
        }

        // Single value case
        if (!pos)
        {
            // Return the single value
            return m_pD->m_values[index].m_float;
        }

        // Multivalue case
        check( pos->m_pD->m_parameter==index );

        if ( index<int(m_pD->m_multiValues.size()))
        {
            const auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                return it->second.m_float;
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        return m_pD->m_values[index].m_float;
    }


	//---------------------------------------------------------------------------------------------
    void Parameters::SetFloatValue( int index, float value,
                                    const Ptr<const RangeIndex>& pos )
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_FLOAT );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_FLOAT )
        {
            return;
        }

        // Single value case
        if (!pos)
        {
            // Clear multivalue, if set.
            if (index<int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues[index].clear();
            }

            m_pD->m_values[index].m_float = value;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter==index );

            if ( index>=int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues.resize(index+1);
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                it->second.m_float = value;
            }
            else
            {
                PARAMETER_VALUE valueData;
                valueData.m_float = value;
                m.insert( std::make_pair<>( pos->m_pD->m_values, valueData ) );
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    void Parameters::GetColourValue( int index, float* pR, float* pG, float* pB,
                                     const Ptr<const RangeIndex>& pos ) const
    {
        check( index>=0 && index<(int)m_pD->m_values.size() );
        check( GetType(index)== PARAMETER_TYPE::T_COLOUR );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_COLOUR )
        {
            return;
        }

        // Single value case
        if (!pos)
        {
            // Return the single value
            if (pR) *pR = m_pD->m_values[index].m_colour[0];
            if (pG) *pG = m_pD->m_values[index].m_colour[1];
            if (pB) *pB = m_pD->m_values[index].m_colour[2];
            return;
        }

        // Multivalue case
        check( pos->m_pD->m_parameter==index );

        if ( index<int(m_pD->m_multiValues.size()))
        {
            const auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                if (pR) *pR = it->second.m_colour[0];
                if (pG) *pG = it->second.m_colour[1];
                if (pB) *pB = it->second.m_colour[2];
                return;
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        if (pR) *pR = m_pD->m_values[index].m_colour[0];
        if (pG) *pG = m_pD->m_values[index].m_colour[1];
        if (pB) *pB = m_pD->m_values[index].m_colour[2];
        return;
    }


    //---------------------------------------------------------------------------------------------
    void Parameters::SetColourValue( int index, float r, float g, float b,
                                     const Ptr<const RangeIndex>& pos )
    {
        check( index>=0 && index<(int)m_pD->m_values.size() );
        check( GetType(index)== PARAMETER_TYPE::T_COLOUR );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_COLOUR )
        {
            return;
        }

        // Single value case
        if (!pos)
        {
            // Clear multivalue, if set.
            if (index<int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues[index].clear();
            }

            m_pD->m_values[index].m_colour[0] = r;
            m_pD->m_values[index].m_colour[1] = g;
            m_pD->m_values[index].m_colour[2] = b;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter==index );

            if ( index>=int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues.resize(index+1);
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                it->second.m_colour[0] = r;
                it->second.m_colour[1] = g;
                it->second.m_colour[2] = b;
            }
            else
            {
                PARAMETER_VALUE valueData;
                valueData.m_colour[0] = r;
                valueData.m_colour[1] = g;
                valueData.m_colour[2] = b;
                m.insert( std::make_pair<>( pos->m_pD->m_values, valueData ) );
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    EXTERNAL_IMAGE_ID Parameters::GetImageValue( int index ) const
    {
        check( index >= 0 && index < (int)m_pD->m_values.size() );
        check( GetType( index ) == PARAMETER_TYPE::T_IMAGE );

        return m_pD->m_values[index].m_image;
    }


    //---------------------------------------------------------------------------------------------
    void Parameters::SetImageValue( int index, EXTERNAL_IMAGE_ID id )
    {
        check( index >= 0 && index < (int)m_pD->m_values.size() );
        check( GetType( index ) == PARAMETER_TYPE::T_IMAGE );

        m_pD->m_values[index].m_image = id;
    }


    //---------------------------------------------------------------------------------------------
    const char* Parameters::GetStringValue( int index, const Ptr<const RangeIndex>& pos ) const
    {
        check( index >= 0 && index < (int)m_pD->m_values.size() );
        check( GetType( index ) == PARAMETER_TYPE::T_STRING );

        // Early out in case of invalid parameters
        if ( index < 0 || index >= (int)m_pD->m_values.size() ||
             GetType( index ) != PARAMETER_TYPE::T_STRING )
        {
            return "";
        }

        // Single value case
        if ( !pos )
        {
            // Return the single value
            return m_pD->m_values[index].m_text;
        }

        // Multivalue case
        check( pos->m_pD->m_parameter == index );

        if ( index < int( m_pD->m_multiValues.size() ) )
        {
            const auto& m = m_pD->m_multiValues[index];
            auto it = m.find( pos->m_pD->m_values );
            if ( it != m.end() )
            {
                return it->second.m_text;
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        return m_pD->m_values[index].m_text;
    }


    //---------------------------------------------------------------------------------------------
    void Parameters::SetStringValue( int index, const char* value, const Ptr<const RangeIndex>& pos )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        check( index >= 0 && index < (int)m_pD->m_values.size() );
        check( GetType( index ) == PARAMETER_TYPE::T_FLOAT );

        // Early out in case of invalid parameters
        if ( index < 0 || index >= (int)m_pD->m_values.size() ||
             GetType( index ) != PARAMETER_TYPE::T_FLOAT )
        {
            return;
        }

        int len = std::min( MUTABLE_MAX_STRING_PARAM_LENGTH,
                            int( strnlen( value, MUTABLE_MAX_STRING_PARAM_LENGTH ) ) );

        // Single value case
        if ( !pos )
        {
            // Clear multivalue, if set.
            if ( index < int( m_pD->m_multiValues.size() ) )
            {
                m_pD->m_multiValues[index].clear();
            }

            //m_pD->m_values[index].m_float = value;
			FMemory::Memcpy( m_pD->m_values[index].m_text, value, len );
            m_pD->m_values[index].m_text[len] = 0;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter == index );

            if ( index >= int( m_pD->m_multiValues.size() ) )
            {
                m_pD->m_multiValues.resize( index + 1 );
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find( pos->m_pD->m_values );
            if ( it != m.end() )
            {
                //it->second.m_float = value;
				FMemory::Memcpy( it->second.m_text, value, len );
                it->second.m_text[len] = 0;
            }
            else
            {
                PARAMETER_VALUE valueData;
                //valueData.m_float = value;
				FMemory::Memcpy( valueData.m_text, value, len );
                valueData.m_text[len] = 0;
                m.insert( std::make_pair<>( pos->m_pD->m_values, valueData ) );
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    PROJECTOR Parameters::Private::GetProjectorValue( int index,
                                                      const Ptr<const RangeIndex>& pos ) const
    {

        const PROGRAM& program = m_pModel->GetPrivate()->m_program;

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_values.size()
			 || 
			 index >= (int)program.m_parameters.size()
             ||
             program.m_parameters[index].m_type != PARAMETER_TYPE::T_PROJECTOR )
        {
			check(false);
            return PROJECTOR();
        }

        const PROJECTOR* result = nullptr;

        // Single value case
        if (!pos)
        {
            // Return the single value
            result = &m_values[index].m_projector;
        }

        // Multivalue case
        if (!result)
        {
            check( pos->m_pD->m_parameter==index );

            if ( index<int(m_multiValues.size()))
            {
                const auto& m = m_multiValues[index];
                auto it = m.find(pos->m_pD->m_values);
                if (it!=m.end())
                {
                    result = &it->second.m_projector;
                }
            }
        }

        // Multivalue parameter, but no multivalue set. Return single value.
        if (!result)
        {
            result = &m_values[index].m_projector;
        }

        return *result;
    }


	//---------------------------------------------------------------------------------------------
	void Parameters::GetProjectorValue( int index,
                                        PROJECTOR_TYPE* pType,
										float* pPosX, float* pPosY, float* pPosZ,
										float* pDirX, float* pDirY, float* pDirZ,
										float* pUpX, float* pUpY, float* pUpZ,
                                        float* pScaleX, float* pScaleY, float* pScaleZ,
                                        float* pProjectionAngle,
                                        const Ptr<const RangeIndex>& pos ) const
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_PROJECTOR );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_PROJECTOR )
        {
            return;
        }

        PROJECTOR result = m_pD->GetProjectorValue( index, pos );

        // Copy results
        if (pType) *pType = result.type;

        if (pPosX) *pPosX = result.position[0];
        if (pPosY) *pPosY = result.position[1];
        if (pPosZ) *pPosZ = result.position[2];

        if (pDirX) *pDirX = result.direction[0];
        if (pDirY) *pDirY = result.direction[1];
        if (pDirZ) *pDirZ = result.direction[2];

        if (pUpX) *pUpX = result.up[0];
        if (pUpY) *pUpY = result.up[1];
        if (pUpZ) *pUpZ = result.up[2];

        if (pScaleX) *pScaleX = result.scale[0];
        if (pScaleY) *pScaleY = result.scale[1];
        if (pScaleZ) *pScaleZ = result.scale[2];

        if (pProjectionAngle) *pProjectionAngle = result.projectionAngle;
    }


	//---------------------------------------------------------------------------------------------
	void Parameters::SetProjectorValue( int index,
										float posX, float posY, float posZ,
										float dirX, float dirY, float dirZ,
										float upX, float upY, float upZ,
                                        float scaleX, float scaleY, float scaleZ,
                                        float projectionAngle,
                                        const Ptr<const RangeIndex>& pos )
	{
		check( index>=0 && index<(int)m_pD->m_values.size() );
		check( GetType(index)== PARAMETER_TYPE::T_PROJECTOR );

        // Early out in case of invalid parameters
        if ( index < 0
             ||
             index >= (int)m_pD->m_values.size()
             ||
             GetType(index) != PARAMETER_TYPE::T_PROJECTOR )
        {
            return;
        }

        PROJECTOR* result = nullptr;

        // Single value case
        if (!pos)
        {
            // Clear multivalue, if set.
            if (index<int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues[index].clear();
            }

            result = &m_pD->m_values[index].m_projector;
        }

        // Multivalue case
        else
        {
            check( pos->m_pD->m_parameter==index );

            if ( index>=int(m_pD->m_multiValues.size()))
            {
                m_pD->m_multiValues.resize(index+1);
            }

            auto& m = m_pD->m_multiValues[index];
            auto it = m.find(pos->m_pD->m_values);
            if (it!=m.end())
            {
                result = &it->second.m_projector;
            }
            else
            {
                auto it2 = m.insert( std::make_pair<>( pos->m_pD->m_values, PARAMETER_VALUE() ) );
                result = &it2.first->second.m_projector;
            }
        }

        check( result );

        // Parameters cannot change the projector type anymore
        result->type = PROJECTOR_TYPE::COUNT;
        if (m_pD->m_pModel)
        {
            const auto& program = m_pD->m_pModel->GetPrivate()->m_program;
            result->type = program.m_parameters[index].m_defaultValue.m_projector.type;
        }

        result->position[0] = posX;
        result->position[1] = posY;
        result->position[2] = posZ;

        result->direction[0] = dirX;
        result->direction[1] = dirY;
        result->direction[2] = dirZ;

        result->up[0] = upX;
        result->up[1] = upY;
        result->up[2] = upZ;

        result->scale[0] = scaleX;
        result->scale[1] = scaleY;
        result->scale[2] = scaleZ;

        result->projectionAngle = projectionAngle;
    }


    //---------------------------------------------------------------------------------------------
    bool Parameters::HasSameValue( int thisParamIndex,
                                   const ParametersPtrConst& other,
                                   int otherParamIndex ) const
    {
        if ( GetType(thisParamIndex) != other->GetType(otherParamIndex) )
        {
            return false;
        }

        if ( !(m_pD->m_values[thisParamIndex]==other->m_pD->m_values[otherParamIndex] ) )
        {
            return false;
        }

        size_t thisNumMultiValues = 0;
        bool thisHasMultiValues = int(m_pD->m_multiValues.size()) > thisParamIndex;
        if (thisHasMultiValues)
        {
            thisNumMultiValues = m_pD->m_multiValues[thisParamIndex].size();
        }

        size_t otherNumMultiValues = 0;
        bool otherHasMultiValues = int(other->m_pD->m_multiValues.size()) > otherParamIndex;
        if (otherHasMultiValues)
        {
            otherNumMultiValues = other->m_pD->m_multiValues[otherParamIndex].size();
        }

        if ( thisNumMultiValues != otherNumMultiValues )
        {
            return false;
        }

        if ( thisHasMultiValues
             &&
             otherHasMultiValues
             &&
             m_pD->m_multiValues[thisParamIndex]!=other->m_pD->m_multiValues[otherParamIndex] )
        {
            return false;
        }

        return true;
    }


	//---------------------------------------------------------------------------------------------
	int Parameters::Private::Find( const char* strName ) const
	{
		const PROGRAM& program = m_pModel->GetPrivate()->m_program;

		int result = -1;

		for( int i=0; result<0 && i<(int)program.m_parameters.size(); ++i )
		{
			const PARAMETER_DESC& p = program.m_parameters[i];

			if ( p.m_name == strName )
			{
				result = i;
			}
		}

		return result;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    RangeIndex::RangeIndex()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    RangeIndex::~RangeIndex()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    RangeIndex::Private* RangeIndex::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    int RangeIndex::GetRangeCount() const
    {
        return int( m_pD->m_values.size() );
    }


    //---------------------------------------------------------------------------------------------
    const char* RangeIndex::GetRangeName( int index ) const
    {
        check( index >= 0 && index < GetRangeCount() );
        check( index < int(m_pD->m_pParameters->GetPrivate()->m_values.size()) );
        ModelPtrConst m_pModel = m_pD->m_pParameters->GetPrivate()->m_pModel;
        check( m_pModel );
        check( index < int(m_pModel->GetPrivate()->m_program.m_ranges.size()) );

        return m_pModel->GetPrivate()->m_program.m_ranges[index].m_name.c_str();
    }


    //---------------------------------------------------------------------------------------------
    const char* RangeIndex::GetRangeUid( int index ) const
    {
        check( index >= 0 && index < GetRangeCount() );
        check( index < int(m_pD->m_pParameters->GetPrivate()->m_values.size()) );
        ModelPtrConst m_pModel = m_pD->m_pParameters->GetPrivate()->m_pModel;
        check( m_pModel );
        check( index < int(m_pModel->GetPrivate()->m_program.m_ranges.size()) );

        return m_pModel->GetPrivate()->m_program.m_ranges[index].m_uid.c_str();
    }


    //---------------------------------------------------------------------------------------------
    void RangeIndex::SetPosition( int index, int position )
    {
        check( index >= 0 && index < GetRangeCount() );
        if ( index >= 0 && index < GetRangeCount() )
        {
            m_pD->m_values[index] = position;
        }
    }


    //---------------------------------------------------------------------------------------------
    int RangeIndex::GetPosition( int index ) const
    {
        check( index >= 0 && index < GetRangeCount() );
        if ( index >= 0 && index < GetRangeCount() )
        {
            return m_pD->m_values[index];
        }
        return 0;
    }

}
