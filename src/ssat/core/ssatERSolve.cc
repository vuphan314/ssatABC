/**CFile****************************************************************

  FileName    [SsatERSolver.cc]

  SystemName  [ssatQesto]

  Synopsis    [Implementations of member functions for ssatSolver]

  Author      []
  
  Affiliation [NTU]

  Date        [14, Jan., 2017]

***********************************************************************/

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdio.h>
#include <math.h>
#include "ssat/utils/ParseUtils.h"
#include "ssat/core/SolverTypes.h"
#include "ssat/core/Solver.h"
#include "ssat/core/Dimacs.h"
#include "ssat/core/SsatSolver.h"
#include "ssat/mtl/Sort.h"

using namespace Minisat;
using namespace std;

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static bool subsume( const vec<Lit>& , const vec<Lit>& );
extern Ssat_Timer_t timer;

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Solve ER or ERE type of SSAT.]

  Description [Use BDD or Cachet to evaluate weight.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::erSolve2SSAT( Ssat_Params_t * pParams )
{
   if ( _fVerbose ) printParams(pParams);
   _s1->simplify();
   _s2 = pParams->fGreedy ? buildQestoSelector() : buildERSelector();
   if ( pParams->fBdd  ) initClauseNetwork( pParams->fIncre , pParams->fCkt );
   if ( pParams->fSub  ) buildSubsumeTable( *_s1 );
   if ( pParams->fPure ) assertPureLit();

   cout << "--------------------------------------\n";
   // declare and initialize temp variables
   vec<Lit> eLits( _rootVars[0].size() ) , sBkCla;
   vec<int> ClasInd;
   int dropIndex , totalSize , lenBeforeDrop = 0;
   double subvalue;
   bool sat;
   abctime clk = 0 , clk1 = Abc_Clock();
   _erModel.capacity( _rootVars[0].size() ); _erModel.clear();
   // main loop, pseudo code line04-14
   while ( true )  {
      if ( _fTimer ) clk = Abc_Clock();
      sat = _s2->solve();
      if ( _fTimer ) { timer.timeS2 += Abc_Clock()-clk; ++timer.nS2solve; }
      if ( !sat ) { // _s2 UNSAT --> main loop terminate
         printf( "\n  > optimizing assignment to exist vars:\n\t" );
         dumpCla(_erModel);
         break;
      }
      getExistAssignment(eLits); // line05
      if ( pParams->fGreedy ) selectMinClauses(eLits); // line07
      if ( _fTimer ) clk = Abc_Clock();
      sat = _s1->solve(eLits);
      if ( _fTimer ) timer.timeS1 += Abc_Clock()-clk;
      if ( !sat ) { // UNSAT case, line12-13
         if ( _fTimer ) ++timer.nS1_unsat;
         if ( pParams->fMini ) {
            sBkCla.clear();
            miniUnsatCore( _s1->conflict , sBkCla );
            _s2->addClause(sBkCla);
         }
         else _s2->addClause(_s1->conflict);
      }
      else { // SAT case
         if ( _fTimer ) ++timer.nS1_sat;
         dropIndex = eLits.size();
         totalSize = eLits.size();
         if ( _s1->nClauses() == 0 ) {
            Abc_Print( -1 , "  > There is no clause left ...\n" );
            Abc_Print( -1 , "  > Should look at unit assumption to compute value ...\n" );
            Abc_Print( 0  , "  > Under construction ...\n" );
            exit(1);
         }
         if ( _fTimer ) clk = Abc_Clock();
         subvalue = pParams->fBdd ? clauseToNetwork( eLits , totalSize , pParams->fIncre , pParams->fCkt ) : countModels( eLits , totalSize );
         if ( _fTimer ) { timer.timeCt += Abc_Clock()-clk; ++timer.nCount; }
         if ( subvalue == 1 ) { // early termination
            _satPb = subvalue;
            eLits.copyTo( _erModel );
            printf( "\n  > optimizing assignment to exist vars:\n\t" );
            dumpCla(_erModel);
            break;
         }
         if ( subvalue > _satPb ) { // update current solution
            if ( _fVerbose ) {
               printf( "  > find a better solution , value = %f\n" , subvalue );
               Abc_PrintTime( 1, "  > Time consumed" , Abc_Clock() - clk1 );
               fflush(stdout);
            }
            _satPb = subvalue;
            eLits.copyTo(_erModel);
         }
         sBkCla.clear();
         ClasInd.clear();
         if ( pParams->fSub ) collectBkClaERSub( sBkCla , ClasInd , dropIndex ); // clause subsumption: line08
         else                 collectBkClaER(sBkCla);
         if ( _fTimer ) {
            if ( pParams->fSub ) timer.lenSubsume += sBkCla.size();
            else                 timer.lenBase    += sBkCla.size();
            if ( pParams->fDynamic ) lenBeforeDrop = sBkCla.size();
         }
         if ( pParams->fPart ) discardLit( pParams , subvalue , sBkCla , ClasInd ); // partial assignment pruning: line11
         _s2->addClause( sBkCla );
         if ( _fTimer ) {
            timer.lenPartial += sBkCla.size();
            if ( pParams->fDynamic ) { // record average # of dropped lits
               timer.lenDrop += lenBeforeDrop - sBkCla.size();
               if ( !timer.avgDone && timer.nS2solve >= 500 ) { // 500 is a magic number: tune it!
                  timer.avgDone = true;
                  timer.avgDrop = (int)(timer.lenDrop / timer.nS1_sat);
                  printf( "  > average done, avg drop = %d\n" , timer.avgDrop );
               }
            }
         }
      }
   }
}

void
SsatSolver::printParams( Ssat_Params_t * pParams ) const
{
   printf( "  > Using %s for counting, greedy=%s, subsume=%s, partial=%s, dynamic=%s, incremental=%s, circuit=%s, pure=%s\n", 
            pParams->fBdd          ? "bdd":"cachet" , 
            pParams->fGreedy       ? "yes":"no" , 
            pParams->fSub          ? "yes":"no" , 
            pParams->fPart         ? "yes":"no" , 
            pParams->fDynamic      ? "yes":"no" , 
            pParams->fIncre        ? "yes":"no" , 
            pParams->fCkt          ? "yes":"no" , 
            pParams->fPure         ? "yes":"no" );
}

void
SsatSolver::getExistAssignment( vec<Lit> & eLits ) const
{
   assert( _s2->okay() );
   for ( int i = 0 ; i < _rootVars[0].size() ; ++i )
      eLits[i] = ( _s2->modelValue(_rootVars[0][i]) == l_True ) ? mkLit(_rootVars[0][i]) : ~mkLit(_rootVars[0][i]);
}

Solver*
SsatSolver::buildERSelector()
{
   return buildAllSelector();
}

void
SsatSolver::removeDupLit( vec<Lit> & c ) const
{
   Lit p; 
   int i , j;
   
   sort(c);
   for ( i = j = 0 , p = lit_Undef ; i < c.size() ; i++ )
      if ( _s1->value(c[i]) != l_False && c[i] != p )
         c[j++] = p = c[i];
   c.shrink(i-j);
}

bool
SsatSolver::dropLit( const vec<Lit>& parLits, vec<int>& ClasInd, int dropIndex, double& subvalue )
{
   bool dropCla;
   double claValue;
   vec<int> newClasInd;
   
   for ( int i = 0 , n = ClasInd.size() ; i < n ; ++i ) {
      Clause & c = _s1->ca[_s1->clauses[ClasInd[i]]];
      dropCla = false;
      claValue = 1;
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && !dropCla ) {
            for ( int k = dropIndex ; k < parLits.size() ; ++k ) {
               if ( var(c[j]) == var(parLits[k]) ) {
                  dropCla = true;
                  break;
               }
            }
         }
         else if ( isRVar(var(c[j])) ) {
            if ( _s1->value(c[j]) == l_True )
               claValue *= _quan[var(c[j])];
            else
               claValue *= 1 - _quan[var(c[j])];
         }
      }
      if ( dropCla )
         subvalue += claValue;
      else
         newClasInd.push(ClasInd[i]);
   }
   if ( newClasInd.size() == ClasInd.size() )
      return false;
   newClasInd.copyTo(ClasInd);
   if ( subvalue <= _satPb )
      return false;
   return true;
}

void
SsatSolver::collectParLits( vec<Lit> & parLits, vec<int> & ClasInd )
{
   vec<Lit> tmpLits;
   bool block;
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      block = true;
      Clause & c = _s1->ca[_s1->clauses[i]];
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 && _s1->modelValue(c[j]) == l_True ) {
            block = false;
            break;
         }
      }
      if ( block ) {
         ClasInd.push(i);
         for ( int j = 0 ; j < c.size() ; ++j ) {
            if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 )
               tmpLits.push (~c[j]);
         }
      }
   }

   sort(tmpLits);
   Lit p; int i, j;
   int num[500] = {0}, max = -1, maxIndex = -1;
   for (i = j = 0, p = lit_Undef; i < tmpLits.size(); i++)
      if (tmpLits[i] != p) {
         tmpLits[j++] = p = tmpLits[i];
      }
      else 
         num[j-1]++;
   tmpLits.shrink(i - j);
   for(;;) {
      for ( int i = 0 ; i < tmpLits.size() ; ++i )
      {
         if ( num[i] > max ) {
            max = num[i];
            maxIndex = i;
         }
      }
      if ( max == -1 ) break;
      parLits.push(tmpLits[ maxIndex ]);
      num[maxIndex] = -1;
      max = -1;
   }
}

void
SsatSolver::collectBkClaER( vec<Lit> & sBkCla )
{
   bool block;
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      block = true;
      Clause & c = _s1->ca[_s1->clauses[i]];
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 && _s1->modelValue(c[j]) == l_True ) {
            block = false;
            break;
         }
      }
      if ( block ) {
         for ( int j = 0 ; j < c.size() ; ++j ) {
            if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 )
               sBkCla.push (c[j]);
         }
      }
   }
   removeDupLit(sBkCla);
}

/**Function*************************************************************

  Synopsis    [Counting models under an exist assignment]

  Description []
               
  SideEffects [Cachet: variables ids must be consecutive!]

  SeeAlso     []

***********************************************************************/

double
SsatSolver::countModels( const vec<Lit> & sBkCla , int dropIndex )
{
   //return _satPb;
   FILE * file;
   int length = 1024;
   char prob_str[length] , cmdModelCount[length];

   // vec<Lit> assump( sBkCla.size() );
   // for ( int i = 0 ; i < sBkCla.size() ; ++i ) assump[i] = ~sBkCla[i];
   
   // toDimacsWeighted( "temp.wcnf" , assump );
   // toDimacsWeighted( "temp.wcnf" , sBkCla );
   // cout << "dropIndex: " << dropIndex << " , clause: ";
   // dumpCla(sBkCla);
   toDimacsWeighted( "temp.wcnf" , sBkCla , dropIndex );
   sprintf( cmdModelCount , "bin/cachet temp.wcnf > tmp.log");
   if ( system( cmdModelCount ) ) {
      fprintf( stderr , "Error! Problems with cachet execution...\n" );
      exit(1);
   }

   sprintf( cmdModelCount , "cat tmp.log | grep \"Satisfying\" | awk '{print $3}' > satProb.log" );
   system( cmdModelCount );
   
   file = fopen( "satProb.log" , "r" );
   if ( file == NULL ) {
      fprintf( stderr , "Error! Problems with reading probability from \"satProb.log\"\n" );
      exit(1);
   }
   fgets( prob_str , length , file );
   fclose( file );
   return atof(prob_str);
}

/**Function*************************************************************

  Synopsis    [SsatSolve write to cnf file with weighted variables]

  Description [For Weighted Model Counting use.]
               
  SideEffects [Using S to switch which solver to write.]

  SeeAlso     []

***********************************************************************/

static Var
mapVar( Var x , vec<Var> & map , Var & max )
{
   if ( map.size() <= x || map[x] == -1) {
      map.growTo( x + 1 , -1 );
      map[x] = max++;
   }
   return map[x];
}

static double
mapWeight( Var x , vec<double> & weights , double weight )
{
   if ( weights.size() <= x || weights[x] != weight ) {
      weights.growTo( x + 1 , -1 );
      weights[x] = weight;
   }
   return weights[x];
}

void
SsatSolver::toDimacs( FILE * f , Clause & c , vec<Var> & map , Var & max , int dropIndex )
{
   //Solver * S = _s1;
   //if ( S->satisfied(c) ) return;

   for ( int i = 0 ; i < c.size() ; ++i )
      fprintf( f , "%s%d " , sign(c[i]) ? "-" : "" , mapVar( var(c[i]) , map , max ) + 1 );
   fprintf( f , "0\n" );
}

void
SsatSolver::toDimacsWeighted( FILE * f , vec<double> & weights , Var & max )
{
   for ( int i = 0 ; i < weights.size() ; ++i )
      fprintf( f , "w %d %f\n" , i + 1 , weights[i] );
}

void
SsatSolver::toDimacsWeighted( const char * file , const vec<Lit> & assumps , int dropIndex )
{
   FILE * f = fopen( file , "wr" );
   if ( f == NULL )
      fprintf( stderr , "could not open file %s\n" , file ), exit(1);
   toDimacsWeighted( f , assumps , dropIndex );
   fclose(f);
}

void
SsatSolver::toDimacsWeighted( FILE * f , const vec<Lit> & assumps , int dropIndex )
{
   Solver * S = _s1;

   vec<bool> drop( _s1->nVars() , false );
   for ( int i = dropIndex ; i < assumps.size() ; ++i ) drop[var(assumps[i])] = true;
   
   // Handle case when solver is in contradictory state:
   if ( !S->ok ) {
      fprintf( f , "p cnf 1 2\n1 0\n-1 0\n" );
      return;
   }

   Var max = 0, tmpVar;
   int cnt = 0;
   vec<Var> map;
   vec<double> weights;
   bool select = true;
   
   // map var to 0 ~ max
   // map 0 ~ max to original weight
   for ( int i = 0 ; i < S->clauses.size() ; ++i ) {
      Clause& c = S->ca[S->clauses[i]];
      for ( int j = 0 ; j < c.size(); ++j ) {
      // cout << "dropIndex : " << dropIndex << endl;
      // cout << "var : " << var(c[j]) << endl;
         if ( drop[var(c[j])] || isEVar(var(c[j])) && _level[var(c[j])] == 0 && _s1->modelValue(c[j]) == l_True ) {
            select = false;
         }
      }
      if ( select == true ) {
         cnt++;
         Clause& c = S->ca[S->clauses[i]];
         for ( int j = 0 ; j < c.size(); ++j ) {
               tmpVar = mapVar( var(c[j]) , map , max );
               mapWeight( tmpVar, weights, ( isRVar(var(c[j])) ? _quan[var(c[j])] : -1 ) );
         }
      }
      select = true;
   }

   // Assumptions are added as unit clauses:
   cnt += dropIndex;
   for ( int i = 0 ; i < dropIndex ; ++i ) {
      tmpVar = mapVar( var(assumps[i]) , map , max );
      mapWeight( tmpVar , weights , ( isRVar(var(assumps[i])) ? _quan[var(assumps[i])] : -1 ) );
   }

   fprintf(f, "p cnf %d %d\n", max, cnt);

// clause_average = (clause_average * clause_number + cnt) / (clause_number+1); 
// ++clause_number;

   for ( int i = 0 ; i < dropIndex ; ++i ) {
      fprintf( f , "%s%d 0\n" , sign(assumps[i]) ? "-" : "" , mapVar( var(assumps[i]) , map , max ) + 1 );
   }

   // for ( int i = 0 ; i < S->clauses.size() ; ++i )
      // toDimacs( f , S->ca[S->clauses[i]] , map , max , dropIndex );
   select = true;
   for ( int i = 0 ; i < S->clauses.size() ; ++i ) {
      Clause& c = S->ca[S->clauses[i]];
      for ( int j = 0 ; j < c.size(); ++j )
         if ( drop[var(c[j])] || isEVar(var(c[j])) && _level[var(c[j])] == 0 && _s1->modelValue(c[j]) == l_True ) {
            select = false;
         }
      if ( select == true ) {
         toDimacs( f , S->ca[S->clauses[i]] , map , max , dropIndex );
      }
      select = true;
   }

   toDimacsWeighted( f , weights , max );
}

/**Function*************************************************************

  Synopsis    [Assign pure X literals to be true.]

  Description [If some X literal is pure, always deselect.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::assertPureLit()
{
   vec<int> phase( _s1->nVars() , -1 ); // -1: default, 0:pos, 1:neg, 2: both
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      CRef     cr = _s1->clauses[i];
      Clause & c  = _s1->ca[cr];
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( _level[var(c[j])] == 0 && phase[var(c[j])] != 2 ) {
            if ( phase[var(c[j])] == -1 ) 
               phase[var(c[j])] = sign(c[j]) ? 1 : 0;
            else if ( ((bool)phase[var(c[j])]) ^ sign(c[j]) )
               phase[var(c[j])] = 2;
         }
      }
   }
   for ( int i = 0 ; i < _rootVars[0].size() ; ++i ) {
      if ( phase[_rootVars[0][i]] == 0 || phase[_rootVars[0][i]] == 1 )
         _s2->addClause( mkLit( _rootVars[0][i] , (bool)phase[_rootVars[0][i]] ) );
   }
}

/**Function*************************************************************

  Synopsis    [Select a minimal set of clauses.]

  Description [Enable by setting fGreedy to true.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::selectMinClauses( vec<Lit> & eLits )
{
   abctime clk = 0;
   bool sat = false;
   vec<Lit> block , assump;
   block.capacity( _claLits.size() );
   assump.capacity( _claLits.size() );
   while ( true ) {
      block.clear();
      assump.clear();
      for ( int i = 0 ; i < _claLits.size() ; ++i ) {
         if ( _claLits[i] == lit_Undef ) continue;
         ( _s2->modelValue(_claLits[i]) == l_True ) ? block.push(~_claLits[i]) : assump.push(~_claLits[i]);
      }
      _s2->addClause( block );
      if ( _fTimer ) clk = Abc_Clock();
      sat = _s2->solve(assump);
      if ( _fTimer ) timer.timeGd += Abc_Clock()-clk;
      if ( !sat ) break;            // minimal set of clauses obtained
      if ( _fTimer ) ++timer.nGdsolve;
      getExistAssignment( eLits );  // update exist assignment 
   }
}

/**Function*************************************************************

  Synopsis    [Clause subsumption]

  Description [Enable by setting fSub to true]
               
  SideEffects [Cachet: variables ids must be consecutive!]

  SeeAlso     []

***********************************************************************/

void
SsatSolver::collectBkClaERSub( vec<Lit> & sBkCla , vec<int> & ClasInd , int dropIndex )
{
   vec<bool> drop( _s1->nVars() , false );
   for ( int i = dropIndex ; i < _rootVars[0].size() ; ++i ) drop[_rootVars[0][i]] = true;

   bool block;
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      block = true;
      Clause & c = _s1->ca[_s1->clauses[i]];
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( drop[var(c[j])] || isEVar(var(c[j])) && _level[var(c[j])] == 0 && _s1->modelValue(c[j]) == l_True ) {
            block = false;
            break;
         }
      }
      if ( block ) {
         ClasInd.push(i);
         /*
         for ( int j = 0 ; j < c.size() ; ++j ) {
            // cout << ( sign(c[j]) ? "-": "" ) << var(c[j])+1 << " ";
            if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 )
               sBkCla.push (c[j]);
         }
         */
         // cout << "\n";
      }
   }
   bool subsume = false;
   //dumpCla(*_s1);
   for ( int i = 0 ; i < ClasInd.size() ; ++i ) {
      Clause & c = _s1->ca[_s1->clauses[ClasInd[i]]];
      subsume = false;
      for ( set<int>::iterator it = _subsumeTable[ClasInd[i]].begin() ; it != _subsumeTable[ClasInd[i]].end() ; ++it ) {
         for ( int j = 0 ; j < ClasInd.size(); ++j ) {
            if ( ClasInd[j] == *it ) {
               subsume = true;
         //cout << " " << ClasInd[j] << " subsume " << i << '\n';
               break;
            }
         }
         if ( subsume ) break;
      }
      if ( subsume ) continue;
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 )
            sBkCla.push (c[j]);
      }
   }
   removeDupLit(sBkCla);
}

void
SsatSolver::buildSubsumeTable( Solver & S )
{
   int numOfClas = S.nClauses();
   assert( numOfClas > 1 );
   _subsumeTable.growTo( numOfClas );

   for ( int i = 0 ; i < S.nClauses() ; ++i ) {
      Clause &c = S.ca[S.clauses[i]];
      for ( int j = 0; j < S.nClauses() ; ++j ) {
         if ( j == i ) continue;
         if ( subsume ( c , S.ca[S.clauses[j]] ) ) _subsumeTable[i].insert(j);
      }
   }
}

bool
SsatSolver::checkSubsumption( Solver & S ) const
{
   vec<int> selClaInd;
   bool     select;
   for ( int i = 0 ; i < S.nClauses() ; ++i ) {
      CRef     cr = S.clauses[i];
      Clause & c  = S.ca[cr];
      select      = true;
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 && S.modelValue(c[j]) == l_True ) {
            select = false;
            break;
         }
      }
      if ( select ) selClaInd.push(i);
   }
   vec<bool> sub( selClaInd.size() , false );
   int lenOld = getLearntClaLen( S , selClaInd , sub );
   for ( int i = 0 ; i < selClaInd.size() ; ++i ) {
      CRef     cr  = S.clauses[selClaInd[i]];
      Clause & c1  = S.ca[cr];
      for ( int j = 0 ; j < selClaInd.size() ; ++j ) {
         if ( j == i ) continue;
         CRef     cr  = S.clauses[selClaInd[j]];
         Clause & c2  = S.ca[cr];
         if ( subsume( c1 , c2 ) ) {
            sub[i] = true;
            break;
         }
      }
   }
   int numSub = 0;
   for ( int i = 0 ; i < sub.size() ; ++i ) if ( sub[i] ) ++numSub;
   int lenSub = getLearntClaLen( S , selClaInd , sub );
   //printf( "[INFO] number of subsume = %3d (out of %3d clauses)\n" , numSub , sub.size() );
   if ( lenSub < lenOld ) {
      printf( "[INFO] length of learnt  = %3d (original %3d lits)\n" , lenSub , lenOld );
      fflush(stdout);
      return true;
   }
   return false;
}

bool
SsatSolver::subsume( const Clause & c1 , const Clause & c2 ) const
{
   // return true iff c2 subsumes c1
   bool noRVar = true;
   for ( int i = 0 ; i < c2.size() ; ++i ) {
      if ( isEVar(var(c2[i])) ) continue;
      noRVar = false;
      bool find = false;
      for ( int j = 0 ; j < c1.size() ; ++j ) {
         if ( isEVar(var(c1[j])) ) continue;
         if ( c1[j] == c2[i] ) {
            find = true;
            break;
         }
      }
      if ( !find ) return false;
   }
   return !noRVar;
}

int
SsatSolver::getLearntClaLen( Solver & S , const vec<int>& selClaInd , const vec<bool>& sub ) const
{
   vec<Lit> learnt;
   for ( int i = 0 ; i < selClaInd.size() ; ++i ) {
      if ( sub[i] ) continue;
      CRef     cr = S.clauses[selClaInd[i]];
      Clause & c  = S.ca[cr];
      fflush(stdout);
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( isEVar(var(c[j])) && _level[var(c[j])] == 0 ) {
            bool unique = true;
            for ( int k = 0 ; k < learnt.size() ; ++k ) {
               if ( learnt[k] == c[j] ) {
                  unique = false;
                  break;
               }
            }
            if ( unique ) learnt.push( c[j] );
         }
      }
   }
   return learnt.size();
}

/**Function*************************************************************

  Synopsis    [Partial assignment pruning.]

  Description [Enable by setting fPart to true.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::discardLit( Ssat_Params_t * pParams , double subvalue , vec<Lit> & sBkCla , vec<int> & ClasInd )
{
   abctime clk = 0;
   int dropIndex = sBkCla.size();
   vec<Lit> parLits;
   sBkCla.copyTo( parLits );
   for ( int i = 0 ; i < parLits.size() ; ++i ) parLits[i] = ~parLits[i];
   if ( dropIndex >= 1 ) {
      if ( pParams->fDynamic && timer.avgDone ) dropIndex -= timer.avgDrop;
      else                                      dropIndex -= 1;
      while ( !dropLit( parLits , ClasInd , dropIndex , subvalue ) ) --dropIndex;
      if ( _fTimer ) clk = Abc_Clock();
      subvalue = pParams->fBdd ? clauseToNetwork( parLits , dropIndex , pParams->fIncre , pParams->fCkt ) : countModels( parLits , dropIndex );
      if ( _fTimer ) { timer.timeCt += Abc_Clock()-clk; ++timer.nCount; }
      if ( subvalue <= _satPb ) { // success, keep dropping 1 by 1
         while ( true ) {
            --dropIndex;
            if ( _fTimer ) clk = Abc_Clock();
            subvalue  = pParams->fBdd ? clauseToNetwork( parLits , dropIndex , pParams->fIncre , pParams->fCkt ) : countModels( parLits , dropIndex );
            if ( _fTimer ) { timer.timeCt += Abc_Clock()-clk; ++timer.nCount; }
            if ( subvalue > _satPb ) break;
         }
         ++dropIndex;
      }
      else { // fail, undo dropping 1 by 1 
         while ( true ) {
            ++dropIndex;
            if ( _fTimer ) clk = Abc_Clock();
            subvalue  = pParams->fBdd ? clauseToNetwork( parLits , dropIndex , pParams->fIncre , pParams->fCkt ) : countModels( parLits , dropIndex );
            if ( _fTimer ) { timer.timeCt += Abc_Clock()-clk; ++timer.nCount; }
            if ( subvalue <= _satPb ) break;
         }
      }
   }
   sBkCla.clear();
   for ( int i = 0 ; i < dropIndex; ++i ) sBkCla.push( ~parLits[i] );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
