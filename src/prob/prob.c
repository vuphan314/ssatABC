/**CFile****************************************************************

  FileName    [prob.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [prob : probabilistic design operation.]

  Synopsis    [Command file.]

  Author      [Nian-Ze Lee]
  
  Affiliation [NTU]

  Date        [May 14, 2016.]

***********************************************************************/

#include <stdlib.h>

#include "base/main/mainInt.h"
#include "aig/saig/saig.h"
#include "proof/abs/abs.h"
#include "sat/bmc/bmc.h"
#include "prob.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int Pb_CommandTest         ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandGenProb      ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandDistill      ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandWritePBN     ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandWriteWMC     ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandWriteSSAT    ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandBddSp        ( Abc_Frame_t * pAbc , int argc , char ** argv );
static int Pb_CommandProbMiter    ( Abc_Frame_t * pAbc , int argc , char ** argv );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [start prob package]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void 
Pb_Init( Abc_Frame_t * pAbc )
{
   // register commands
   Cmd_CommandAdd( pAbc , "z prob" , "pb_test"      , Pb_CommandTest         , 0 );
   Cmd_CommandAdd( pAbc , "z prob" , "genprob"      , Pb_CommandGenProb      , 1 );
   Cmd_CommandAdd( pAbc , "z prob" , "distill"      , Pb_CommandDistill      , 1 );
   Cmd_CommandAdd( pAbc , "z prob" , "write_pbn"    , Pb_CommandWritePBN     , 0 );
   Cmd_CommandAdd( pAbc , "z prob" , "write_wmc"    , Pb_CommandWriteWMC     , 0 );
   Cmd_CommandAdd( pAbc , "z prob" , "write_ssat"   , Pb_CommandWriteSSAT    , 0 );
   Cmd_CommandAdd( pAbc , "z prob" , "bddsp"        , Pb_CommandBddSp        , 1 );
   Cmd_CommandAdd( pAbc , "z prob" , "probmiter"    , Pb_CommandProbMiter    , 1 );
	// initialize random seed
	srand( time(0) );
}

/**Function*************************************************************

  Synopsis    [stop prob package]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void 
Pb_End( Abc_Frame_t * pAbc )
{
}

/**Function*************************************************************

  Synopsis    [generate a floating number in [0,1]]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

float
randProb()
{
   return ( (float)rand() / (float)RAND_MAX );
}

/**Function*************************************************************

  Synopsis    [test interface]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int 
Pb_CommandTest( Abc_Frame_t * pAbc , int argc , char ** argv )
{
	// modified for testing git
   Abc_Ntk_t * pNtk;
	Abc_Obj_t * pObj;
   int i;

   pNtk = Abc_FrameReadNtk( pAbc );
	Abc_NtkForEachPi( pNtk , pObj , i )
	{
		printf( "HIHI Obj name = %s , float = %f\n" , Abc_ObjName( pObj ) , pObj->dTemp );
	}
	return 0;
}

/**Function*************************************************************

  Synopsis    [generate probabilistic design]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandGenProb( Abc_Frame_t * pAbc , int argc , char ** argv )
{
	Abc_Ntk_t * pNtk;
	float Error , Defect;
	int c;
   
	Error = Defect = 0.5;
   Extra_UtilGetoptReset();
	while ( ( c = Extra_UtilGetopt( argc, argv, "EDh" ) ) != EOF ) {
      switch ( c )
      {
         case 'E':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-E\" should be followed by a float.\n" );
                goto usage;
            }
            Error = (float)atof( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( Error < 0.0 ) goto usage;
            break;
         case 'D':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-D\" should be followed by a float.\n" );
                goto usage;
            }
            Defect = (float)atof( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( Defect < 0.0 ) goto usage;
            break;
         case 'h':
            goto usage;
         default:
            goto usage;
      }
   }
   pNtk = Abc_FrameReadNtk( pAbc );
	
	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( !Abc_NtkIsComb( pNtk ) ) {
      Abc_NtkMakeComb( pNtk , 0 );
      Abc_Print( 0 , "The network is made comb.\n" );
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		pNtk = Abc_NtkStrash( pNtk , 0 , 1 , 0 );
      Abc_FrameReplaceCurrentNetwork( pAbc , pNtk );
		Abc_Print( 0 , "The network is strashed and replaced.\n" );
	}
	Pb_GenProbNtk( pNtk , Error , Defect );
	return 0;

usage:
    Abc_Print( -2, "usage: genprob [-E error] [-D defect] [-h]\n" );
    Abc_Print( -2, "\t            generate probabilistic design with specified error/defect rates\n" );
    Abc_Print( -2, "\t-E error  : the error  rate of a gate   [default = %f]\n" ,  0.5 );
    Abc_Print( -2, "\t-D defect : the defect rate of a design [default = %f]\n" ,  0.5 );
    Abc_Print( -2, "\t-h        : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [distill probabilistic design (convert into SPBN)]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandDistill( Abc_Frame_t * pAbc , int argc , char ** argv )
{
	Abc_Ntk_t * pNtk , * pNtkRes;
	int c;
   
   Extra_UtilGetoptReset();
	while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF ) {
      switch ( c )
      {
         case 'h':
            goto usage;
         default:
            goto usage;
		}
	}
   
	pNtk = Abc_FrameReadNtk( pAbc );
	
	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( !Abc_NtkIsComb( pNtk ) ) {
      Abc_NtkMakeComb( pNtk , 0 );
      Abc_Print( 0 , "The network is made comb.\n" );
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}

	pNtkRes = Pb_DistillNtk( pNtk );
	if ( pNtkRes ) {
		Abc_FrameReplaceCurrentNetwork( pAbc , pNtkRes );
      Abc_Print( -2 , "The network is distilled and replaced.\n" );
	}
	return 0;

usage:
    Abc_Print( -2, "usage: distill [-h]\n" );
    Abc_Print( -2, "\t            convert probabilistic design into SPBN\n" );
    Abc_Print( -2, "\t-h        : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [write out probabilistic design]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandWritePBN( Abc_Frame_t * pAbc , int argc , char ** argv )
{
   Abc_Ntk_t * pNtk;
   char ** pArgvNew;
	char *  FileName;
	int     nArgcNew , c;

	pNtk = Abc_FrameReadNtk( pAbc );

   Extra_UtilGetoptReset();
   while ( ( c = Extra_UtilGetopt( argc, argv, "h" ) ) != EOF ) {
      switch ( c )
      {
         case 'h':
            goto usage;
         default:
            goto usage;
	   }
   }

   pArgvNew = argv + globalUtilOptind;
   nArgcNew = argc - globalUtilOptind;
   if ( nArgcNew != 1 ) {
      Abc_Print( -1 , "There is no file name.\n" );
      return 1;
   }
   FileName = pArgvNew[0];

	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}

   Pb_WritePBN( pNtk , FileName );
	Abc_Print( -2 , "File %s is written.\n" , FileName );

   return 0;
usage:
    Abc_Print( -2 , "usage   : write_pbn [-h] <file>\n" );
    Abc_Print( -2 , "\t        write out probabilistic design as a PBN file\n" );
    Abc_Print( -2 , "\t-h    : print the command usage\n");
    Abc_Print( -2 , "\tfile  : the name of output file\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [write out cnf file for cachet to calculate probability]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandWriteWMC( Abc_Frame_t * pAbc , int argc , char ** argv )
{
   Abc_Ntk_t * pNtk;
   char ** pArgvNew;
	char *  FileName;
	int     nArgcNew , numPo , fModel , c;

	pNtk   = Abc_FrameReadNtk( pAbc );
	numPo  = 0;
	fModel = 0;

   Extra_UtilGetoptReset();
   while ( ( c = Extra_UtilGetopt( argc, argv, "Omh" ) ) != EOF ) {
      switch ( c )
      {
         case 'O':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-O\" should be followed by an integer.\n" );
                goto usage;
            }
            numPo = atoi( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( numPo < 0 ) goto usage;
            break;
         case 'm':
				fModel ^= 1;
				break;
         case 'h':
            goto usage;
         default:
            goto usage;
	   }
   }

   pArgvNew = argv + globalUtilOptind;
   nArgcNew = argc - globalUtilOptind;
   if ( nArgcNew != 1 ) {
      Abc_Print( -1 , "There is no file name.\n" );
      return 1;
   }
   FileName = pArgvNew[0];

	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( numPo > Abc_NtkPoNum( pNtk )-1 ) {
	   Abc_Print( -1 , "num(%d) should be less than #PO(%d).\n" , numPo , Abc_NtkPoNum( pNtk ) );
		goto usage;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}

   Pb_WriteWMC( pNtk , FileName , numPo , fModel );
	Abc_Print( -2 , "File %s is written.\n" , FileName );

   return 0;
usage:
    Abc_Print( -2 , "usage    : write_wmc [-O <num>] [-mh] <file>\n" );
    Abc_Print( -2 , "\t         write the weighted model counting file for cachet\n" );
    Abc_Print( -2 , "\t-O num : specify num-th Po to calculate [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-m     : toggles Pi assumptions if the model exists [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-h     : print the command usage\n");
    Abc_Print( -2 , "\tfile   : the name of output file\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [calculate signal probability by bdd]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandBddSp( Abc_Frame_t * pAbc , int argc , char ** argv )
{
   Abc_Ntk_t * pNtk;
	int fAll , fGrp , fVerbose , numPo , numExist , c;

	pNtk     = Abc_FrameReadNtk( pAbc );
	numPo    = 0;
	numExist = 0;
	fAll     = 0;
	fGrp     = 1;
   fVerbose = 1;

   Extra_UtilGetoptReset();
   while ( ( c = Extra_UtilGetopt( argc, argv, "OEagvh" ) ) != EOF ) {
      switch ( c )
      {
         case 'O':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-O\" should be followed by an integer.\n" );
                goto usage;
            }
            numPo = atoi( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( numPo < 0 ) goto usage;
            break;
         case 'E':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-E\" should be followed by an integer.\n" );
                goto usage;
            }
            numExist = atoi( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( numExist < 0 ) goto usage;
            break;
			case 'a':
				fAll ^= 1;
			   break;
			case 'g':
				fGrp ^= 1;
			   break;
			case 'v':
				fVerbose ^= 1;
			   break;
         case 'h':
            goto usage;
         default:
            goto usage;
	   }
   }

	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( numPo > Abc_NtkPoNum( pNtk )-1 ) {
	   Abc_Print( -1 , "numPo(%d) should be less than #PO(%d).\n" , numPo , Abc_NtkPoNum( pNtk ) );
		goto usage;
	}
	if ( numExist > Abc_NtkPiNum( pNtk ) ) {
	   Abc_Print( -1 , "numExist(%d) should be less than #PI(%d).\n" , numExist , Abc_NtkPiNum( pNtk ) );
		goto usage;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}
   if ( fAll ) Pb_BddComputeAllSp( pNtk , numExist , fGrp , fVerbose );
	else        Pb_BddComputeSp( pNtk , numPo , numExist , fGrp , fVerbose );

   return 0;
usage:
    Abc_Print( -2 , "usage    : bddsp [-O <num>] [-E <num>] [-agvh]\n" );
    Abc_Print( -2 , "\t         compute signal probability by bdd\n" );
    Abc_Print( -2 , "\t-O num : specify num-th Po to calculate [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-E num : specify the number of exist variables [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-a     : toggles calculating all Po [default = %s]\n" , "no" );
    Abc_Print( -2 , "\t-g     : toggles grouping PI and AI variables [default = %s]\n" , "yes" );
    Abc_Print( -2 , "\t-v     : toggles printing verbose information [default = %s]\n" , "yes" );
    Abc_Print( -2 , "\t-h     : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [write out ssat file for ssat to calculate probability]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandWriteSSAT( Abc_Frame_t * pAbc , int argc , char ** argv )
{
   Abc_Ntk_t * pNtk;
   char ** pArgvNew;
	char *  FileName;
	int     nArgcNew , numPo , numExist , fQdimacs , c;

	pNtk     = Abc_FrameReadNtk( pAbc );
	numPo    = 0;
	numExist = 0;
   fQdimacs = 1;

   Extra_UtilGetoptReset();
   while ( ( c = Extra_UtilGetopt( argc, argv, "OEqh" ) ) != EOF ) {
      switch ( c )
      {
         case 'O':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-O\" should be followed by an integer.\n" );
                goto usage;
            }
            numPo = atoi( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( numPo < 0 ) goto usage;
            break;
         case 'E':
            if ( globalUtilOptind >= argc ) {
                Abc_Print( -1 , "Command line switch \"-E\" should be followed by an integer.\n" );
                goto usage;
            }
            numExist = atoi( argv[globalUtilOptind] );
            globalUtilOptind++;
            if ( numExist < 0 ) goto usage;
            break;
         case 'q':
            fQdimacs ^= 1;
            break;
         case 'h':
            goto usage;
         default:
            goto usage;
	   }
   }

   pArgvNew = argv + globalUtilOptind;
   nArgcNew = argc - globalUtilOptind;
   if ( nArgcNew != 1 ) {
      Abc_Print( -1 , "There is no file name.\n" );
      return 1;
   }
   FileName = pArgvNew[0];

	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( numPo > Abc_NtkPoNum( pNtk )-1 ) {
	   Abc_Print( -1 , "num(%d) should be less than #PO(%d).\n" , numPo , Abc_NtkPoNum( pNtk ) );
		goto usage;
	}
	if ( numExist > Abc_NtkPiNum( pNtk ) ) {
	   Abc_Print( -1 , "num(%d) should be less than #PI(%d).\n" , numExist , Abc_NtkPiNum( pNtk ) );
		goto usage;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}

   Pb_WriteSSAT( pNtk , FileName , numPo , numExist , fQdimacs );
	Abc_Print( -2 , "File %s is written.\n" , FileName );

   return 0;
usage:
    Abc_Print( -2 , "usage    : write_ssat [-O <num>] [-E <num>] [-qh] <file>\n" );
    Abc_Print( -2 , "\t         write the stochastic sat file for ssat\n" );
    Abc_Print( -2 , "\t-O num : specify num-th Po to calculate [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-E num : specify the number of exist variables [default = %d]\n" , 0 );
    Abc_Print( -2 , "\t-q     : toggles .qdimacs file format [default = %d]\n" , 1 );
    Abc_Print( -2 , "\t-h     : print the command usage\n");
    Abc_Print( -2 , "\tfile   : the name of output file\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    [build the miter for probabilistic eq checking]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

int
Pb_CommandProbMiter( Abc_Frame_t * pAbc , int argc , char ** argv )
{
   Abc_Ntk_t * pNtk , * pNtkGolden , * pNtkRes;
	char * pFileName;
	int fMulti , c;

	pNtk     = Abc_FrameReadNtk( pAbc );
	fMulti   = 0;

   Extra_UtilGetoptReset();
   while ( ( c = Extra_UtilGetopt( argc, argv, "mh" ) ) != EOF ) {
      switch ( c )
      {
         case 'm':
				fMulti ^= 1;
				break;
         case 'h':
            goto usage;
         default:
            goto usage;
	   }
   }
	if ( !pNtk ) {
	   Abc_Print( -1 , "Empty network.\n" );
		return 1;
	}
	if ( !Abc_NtkIsStrash( pNtk ) ) {
		Abc_Print( -1 , "Only support strashed networks for now.\n" );
		return 1;
	}
   if ( argc != globalUtilOptind + 1 ) goto usage;
   pFileName  = argv[globalUtilOptind];
   pNtkGolden = Io_Read( pFileName , Io_ReadFileType(pFileName) , 1 , 0 );
	if ( !pNtkGolden ) {
	   Abc_Print( -1 , "Reading golden network failed.\n" );
		return 1;
	}
   pNtkRes = Pb_ProbMiter( pNtk , pNtkGolden , fMulti );
	if ( pNtkRes ) {
		Abc_FrameReplaceCurrentNetwork( pAbc , pNtkRes );
      Abc_Print( -2 , "The prob miter is built.\n" );
	}
   Abc_NtkDelete( pNtkGolden );
   return 0;

usage:
    Abc_Print( -2 , "usage    : probmiter [-mh] <file>\n" );
    Abc_Print( -2 , "\t         build a miter for PEC (current ntk must be distilled)\n" );
    Abc_Print( -2 , "\t-m     : toggles building multi-outputs miter [default = %s]\n" , "no" );
    Abc_Print( -2 , "\t-h     : print the command usage\n");
    Abc_Print( -2 , "\t<file> : the file containing golden ckt\n");
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

