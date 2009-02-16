/*

HyPhy - Hypothesis Testing Using Phylogenies.

Copyright (C) 1997-2008  
Primary Development:
  Sergei L Kosakovsky Pond (sergeilkp@mac.com)
Significant contributions from:
  Spencer V Muse (muse@stat.ncsu.edu)
  Simon DW Frost (sdfrost@ucsd.edu)
  Art FY Poon    (apoon@biomail.ucsd.edu)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "likefunc.h"

#ifdef	_SLKP_LFENGINE_REWRITE_

/*--------------------------------------------------------------------------------------------------*/

void	_LikelihoodFunction::DetermineLocalUpdatePolicy (void)
{
	for (long k = 0; k < theTrees.lLength; k ++)
	{
		long catCount = ((_TheTree*)LocateVar(theTrees(k)))->categoryCount;
		_List * lup = new _List,
			  * mte = new _List;
		
		computedLocalUpdatePolicy.AppendNewInstance (new _SimpleList (catCount,0,0));
		for (long l = 0; l < catCount; l++)
		{
			lup->AppendNewInstance (new _SimpleList);
			mte->AppendNewInstance (new _List);
		}
			
		localUpdatePolicy.AppendNewInstance		 (lup);
		matricesToExponentiate.AppendNewInstance (mte);
	}
}

/*--------------------------------------------------------------------------------------------------*/

void	_LikelihoodFunction::FlushLocalUpdatePolicy (void)
{
	computedLocalUpdatePolicy.Clear();
	localUpdatePolicy.Clear();
	matricesToExponentiate.Clear();
}

//_______________________________________________________________________________________

void	_LikelihoodFunction::ReconstructAncestors (_DataSet &target, bool sample, long segment, long branch)
{
	/*long  i,
	 j,
	 k,
	 m,
	 n,
	 sc,
	 offset = 0,
	 offset2= 0;*/
	
	_DataSetFilter *dsf				= (_DataSetFilter*)dataSetFilterList (theDataFilters(0));	
	_TheTree    	*firstTree		= (_TheTree*)LocateVar(theTrees(0));
	
	target.SetTranslationTable		(dsf->GetData());	
	target.ConvertRepresentations();
	
	computationalResults.Clear();
	PrepareToCompute();
	Compute();				
	
	_AssociativeList *  supportAVL = nil;
	// store conditional probabilities at the root
	if (sample == false)
	{
		_Parameter			doRootSupportFlag = 0.0;
		checkParameter 		(storeRootSupportFlag, doRootSupportFlag, 0.0);
		if (doRootSupportFlag > 0.5)
			supportAVL = new _AssociativeList;
	}
	
	// check if we need to deal with rate variation
	_Matrix			*rateAssignments = nil;
	if  (indexCat.lLength>0)
	{
		rateAssignments = ConstructCategoryMatrix(true,false);
		checkPointer	 (rateAssignments);
	}
	
	long siteOffset			= 0,
		 patternOffset		= 0,
		 sequenceCount		;
	
	for (long i = 0; i<theTrees.lLength; i++)
	{
		_TheTree   *tree		= (_TheTree*)LocateVar(theTrees(i));
		
		dsf = (_DataSetFilter*)dataSetFilterList (theDataFilters(i));
		
		long    catCounter = 0;
		
		if (rateAssignments)
		{
			_SimpleList				pcats;
			PartitionCatVars		(pcats,i);
			catCounter			  = pcats.lLength;
		}
		
		if (i==0)
		{
			tree->AddNodeNamesToDS (&target,false,true,false); // store internal node names in the dataset
			sequenceCount = target.GetNames().lLength;
		}
		else
		{
			if (!tree->Equal(firstTree)) // incompatible likelihood function
			{
				ReportWarning ((_String("Ancestor reconstruction had to ignore part ")&_String(i+1)&" of the likelihood function since it has a different tree topology than the first part."));
				continue;
			}
			_TranslationTable * mtt = target.GetTT()->MergeTables(dsf->GetData()->GetTT());
			if (mtt)
			{
				target.SetTranslationTable		(mtt);	
				DeleteObject					(mtt);
			}
			else
			{
				ReportWarning ((_String("Ancestor reconstruction had to ignore part ")&_String(i+1)&" of the likelihood function since it has a character alphabet incompatible with the first part."));
				continue;
			}
		}
		
		_AVLListX   * nodeMapper	= tree->ConstructNodeToIndexMap(true);
		_List		* expandedMap	= dsf->ComputePatternToSiteMap(),
					*  thisSet;
		
		if (sample)
		{
			thisSet = new _List;
			tree->SampleAncestorsBySequence (dsf, *(_SimpleList*)optimalOrders.lData[i], &tree->GetRoot(), nodeMapper, conditionalInternalNodeLikelihoodCaches[i],
											 *thisSet, nil,*expandedMap,  catCounter?rateAssignments->theData+siteOffset:nil, catCounter);
			
			
			// store the results
		}
		else
		{
			thisSet = tree->RecoverAncestralSequences (dsf, 
														*(_SimpleList*)optimalOrders.lData[i],
														*expandedMap,
														nodeMapper,
														conditionalInternalNodeLikelihoodCaches[i],
														catCounter?rateAssignments->theData+siteOffset:nil, 
														catCounter,
														conditionalTerminalNodeStateFlag[i],
														(_GrowingVector*)conditionalTerminalNodeLikelihoodCaches(i)
														);
														
														
		}
		
		_String * sampledString = (_String*)(*thisSet)(0);
		
		for (long siteIdx = 0; siteIdx<sampledString->sLength; siteIdx++)
			target.AddSite (sampledString->sData[siteIdx]);
		
		for (long seqIdx = 1;seqIdx < sequenceCount; seqIdx++)
		{
			sampledString = (_String*)(*thisSet)(seqIdx);
			for (long siteIdx = 0;siteIdx<sampledString->sLength; siteIdx++)
				target.Write2Site (siteOffset + siteIdx, sampledString->sData[siteIdx]);
		}
		DeleteObject (thisSet);
		DeleteObject (expandedMap);
		nodeMapper->DeleteAll(false);DeleteObject (nodeMapper);
		siteOffset	  += dsf->GetSiteCount();
		patternOffset += dsf->GetSiteCount();
	}
	
		
	target.Finalize();
	target.SetNoSpecies(target.GetNames().lLength);
	
	if (rateAssignments)
		DeleteObject (rateAssignments);
	
	DoneComputing ();
	
}

#endif