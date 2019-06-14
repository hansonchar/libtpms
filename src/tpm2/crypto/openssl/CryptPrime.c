/********************************************************************************/
/*										*/
/*			    Code for prime validation. 				*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*            $Id: CryptPrime.c 1476 2019-06-10 19:32:03Z kgoldman $		*/
/*										*/
/*  Licenses and Notices							*/
/*										*/
/*  1. Copyright Licenses:							*/
/*										*/
/*  - Trusted Computing Group (TCG) grants to the user of the source code in	*/
/*    this specification (the "Source Code") a worldwide, irrevocable, 		*/
/*    nonexclusive, royalty free, copyright license to reproduce, create 	*/
/*    derivative works, distribute, display and perform the Source Code and	*/
/*    derivative works thereof, and to grant others the rights granted herein.	*/
/*										*/
/*  - The TCG grants to the user of the other parts of the specification 	*/
/*    (other than the Source Code) the rights to reproduce, distribute, 	*/
/*    display, and perform the specification solely for the purpose of 		*/
/*    developing products based on such documents.				*/
/*										*/
/*  2. Source Code Distribution Conditions:					*/
/*										*/
/*  - Redistributions of Source Code must retain the above copyright licenses, 	*/
/*    this list of conditions and the following disclaimers.			*/
/*										*/
/*  - Redistributions in binary form must reproduce the above copyright 	*/
/*    licenses, this list of conditions	and the following disclaimers in the 	*/
/*    documentation and/or other materials provided with the distribution.	*/
/*										*/
/*  3. Disclaimers:								*/
/*										*/
/*  - THE COPYRIGHT LICENSES SET FORTH ABOVE DO NOT REPRESENT ANY FORM OF	*/
/*  LICENSE OR WAIVER, EXPRESS OR IMPLIED, BY ESTOPPEL OR OTHERWISE, WITH	*/
/*  RESPECT TO PATENT RIGHTS HELD BY TCG MEMBERS (OR OTHER THIRD PARTIES)	*/
/*  THAT MAY BE NECESSARY TO IMPLEMENT THIS SPECIFICATION OR OTHERWISE.		*/
/*  Contact TCG Administration (admin@trustedcomputinggroup.org) for 		*/
/*  information on specification licensing rights available through TCG 	*/
/*  membership agreements.							*/
/*										*/
/*  - THIS SPECIFICATION IS PROVIDED "AS IS" WITH NO EXPRESS OR IMPLIED 	*/
/*    WARRANTIES WHATSOEVER, INCLUDING ANY WARRANTY OF MERCHANTABILITY OR 	*/
/*    FITNESS FOR A PARTICULAR PURPOSE, ACCURACY, COMPLETENESS, OR 		*/
/*    NONINFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS, OR ANY WARRANTY 		*/
/*    OTHERWISE ARISING OUT OF ANY PROPOSAL, SPECIFICATION OR SAMPLE.		*/
/*										*/
/*  - Without limitation, TCG and its members and licensors disclaim all 	*/
/*    liability, including liability for infringement of any proprietary 	*/
/*    rights, relating to use of information in this specification and to the	*/
/*    implementation of this specification, and TCG disclaims all liability for	*/
/*    cost of procurement of substitute goods or services, lost profits, loss 	*/
/*    of use, loss of data or any incidental, consequential, direct, indirect, 	*/
/*    or special damages, whether under contract, tort, warranty or otherwise, 	*/
/*    arising in any way out of use or reliance upon this specification or any 	*/
/*    information herein.							*/
/*										*/
/*  (c) Copyright IBM Corp. and others, 2016 - 2019				*/
/*										*/
/********************************************************************************/

/* 10.2.16 CryptPrime.c */
#include "Tpm.h"
#include "CryptPrime_fp.h"
//#define CPRI_PRIME
//#include "PrimeTable.h"
#include "CryptPrimeSieve_fp.h"
extern const uint32_t      s_LastPrimeInTable;
extern const uint32_t      s_PrimeTableSize;
extern const uint32_t      s_PrimesInTable;
extern const unsigned char s_PrimeTable[];
extern bigConst            s_CompositeOfSmallPrimes;
/* 10.2.16.1.1 Root2() */
/* This finds ceil(sqrt(n)) to use as a stopping point for searching the prime table. */
static uint32_t
Root2(
      uint32_t             n
      )
{
    int32_t              last = (int32_t)(n >> 2);
    int32_t              next = (int32_t)(n >> 1);
    int32_t              diff;
    int32_t              stop = 10;
    //
    // get a starting point
    for(; next != 0; last >>= 1, next >>= 2);
    last++;
    do
	{
	    next = (last + (n / last)) >> 1;
	    diff = next - last;
	    last = next;
	    if(stop-- == 0)
		FAIL(FATAL_ERROR_INTERNAL);
	} while(diff < -1 || diff > 1);
    if((n / next) > (unsigned)next)
	next++;
    pAssert(next != 0);
    pAssert(((n / next) <= (unsigned)next) && (n / (next + 1) < (unsigned)next));
    return next;
}
/* 10.2.16.1.2 IsPrimeInt() */
/* This will do a test of a word of up to 32-bits in size. */
BOOL
IsPrimeInt(
	   uint32_t            n
	   )
{
    uint32_t            i;
    uint32_t            stop;
    if(n < 3 || ((n & 1) == 0))
	return (n == 2);
    if(n <= s_LastPrimeInTable)
	{
	    n >>= 1;
	    return ((s_PrimeTable[n >> 3] >> (n & 7)) & 1);
	}
    // Need to search
    stop = Root2(n) >> 1;
    // starting at 1 is equivalent to staring at  (1 << 1) + 1 = 3
    for(i = 1; i < stop; i++)
	{
	    if((s_PrimeTable[i >> 3] >> (i & 7)) & 1)
		// see if this prime evenly divides the number
		if((n % ((i << 1) + 1)) == 0)
		    return FALSE;
	}
    return TRUE;
}
/* 10.2.16.1.3 BnIsProbablyPrime() */
/* This function is used when the key sieve is not implemented. This function Will try to eliminate
   some of the obvious things before going on to perform MillerRabin() as a final verification of
   primeness. */
BOOL
BnIsProbablyPrime(
		  bigNum          prime,           // IN:
		  RAND_STATE      *rand            // IN: the random state just
		  //     in case Miller-Rabin is required
		  )
{
#if RADIX_BITS > 32
    if(BnUnsignedCmpWord(prime, UINT32_MAX) <= 0)
#else
	if(BnGetSize(prime) == 1)
#endif
	    return IsPrimeInt(prime->d[0]);
    if(BnIsEven(prime))
	return FALSE;
    if(BnUnsignedCmpWord(prime, s_LastPrimeInTable) <= 0)
	{
	    crypt_uword_t       temp = prime->d[0] >> 1;
	    return ((s_PrimeTable[temp >> 3] >> (temp & 7)) & 1);
	}
    {
	BN_VAR(n, LARGEST_NUMBER_BITS);
	BnGcd(n, prime, s_CompositeOfSmallPrimes);
	if(!BnEqualWord(n, 1))
	    return FALSE;
    }
    return MillerRabin(prime, rand);
}
/* 10.2.16.1.4 MillerRabinRounds() */
/* Function returns the number of Miller-Rabin rounds necessary to give an error probability equal
   to the security strength of the prime. These values are from FIPS 186-3. */
UINT32
MillerRabinRounds(
		  UINT32           bits           // IN: Number of bits in the RSA prime
		  )
{
    if(bits < 511) return 8;    // don't really expect this
    if(bits < 1536) return 5;   // for 512 and 1K primes
    return 4;                   // for 3K public modulus and greater
}
/* 10.2.16.1.5 MillerRabin() */
/* This function performs a Miller-Rabin test from FIPS 186-3. It does iterations trials on the
   number. In all likelihood, if the number is not prime, the first test fails. */
/* Return Values Meaning */
/* TRUE probably prime */
/* FALSE composite */
BOOL
MillerRabin(
	    bigNum           bnW,
	    RAND_STATE      *rand
	    )
{
    BN_MAX(bnWm1);
    BN_PRIME(bnM);
    BN_PRIME(bnB);
    BN_PRIME(bnZ);
    BOOL             ret = FALSE;   // Assumed composite for easy exit
    unsigned int     a;
    unsigned int     j;
    int              wLen;
    int              i;
    int              iterations = MillerRabinRounds(BnSizeInBits(bnW));
    //
    INSTRUMENT_INC(MillerRabinTrials[PrimeIndex]);
    
    pAssert(bnW->size > 1);
    // Let a be the largest integer such that 2^a divides w1.
    BnSubWord(bnWm1, bnW, 1);
    pAssert(bnWm1->size != 0);
    
    // Since w is odd (w-1) is even so start at bit number 1 rather than 0
    // Get the number of bits in bnWm1 so that it doesn't have to be recomputed
    // on each iteration.
    i = bnWm1->size * RADIX_BITS;
    // Now find the largest power of 2 that divides w1
    for(a = 1;
	(a < (bnWm1->size * RADIX_BITS)) &&
	    (BnTestBit(bnWm1, a) == 0);
	a++);
    // 2. m = (w1) / 2^a
    BnShiftRight(bnM, bnWm1, a);
    // 3. wlen = len (w).
    wLen = BnSizeInBits(bnW);
    // 4. For i = 1 to iterations do
    for(i = 0; i < iterations; i++)
	{
	    // 4.1 Obtain a string b of wlen bits from an RBG.
	    // Ensure that 1 < b < w1.
	    // 4.2 If ((b <= 1) or (b >= w1)), then go to step 4.1.
	    while(BnGetRandomBits(bnB, wLen, rand) && ((BnUnsignedCmpWord(bnB, 1) <= 0)
						       || (BnUnsignedCmp(bnB, bnWm1) >= 0)));
	    if(g_inFailureMode)
		return FALSE;
	    
	    // 4.3 z = b^m mod w.
	    // if ModExp fails, then say this is not
	    // prime and bail out.
	    BnModExp(bnZ, bnB, bnM, bnW);
	    
	    // 4.4 If ((z == 1) or (z = w == 1)), then go to step 4.7.
	    if((BnUnsignedCmpWord(bnZ, 1) == 0)
	       || (BnUnsignedCmp(bnZ, bnWm1) == 0))
		goto step4point7;
	    // 4.5 For j = 1 to a  1 do.
	    for(j = 1; j < a; j++)
		{
		    // 4.5.1 z = z^2 mod w.
		    BnModMult(bnZ, bnZ, bnZ, bnW);
		    // 4.5.2 If (z = w1), then go to step 4.7.
		    if(BnUnsignedCmp(bnZ, bnWm1) == 0)
			goto step4point7;
		    // 4.5.3 If (z = 1), then go to step 4.6.
		    if(BnEqualWord(bnZ, 1))
			goto step4point6;
		}
	    // 4.6 Return COMPOSITE.
	step4point6:
	    INSTRUMENT_INC(failedAtIteration[i]);
	    goto end;
	    // 4.7 Continue. Comment: Increment i for the do-loop in step 4.
	step4point7:
	    continue;
	}
    // 5. Return PROBABLY PRIME
    ret = TRUE;
 end:
    return ret;
}
#if ALG_RSA
/* 10.2.16.1.6 RsaCheckPrime() */
/* This will check to see if a number is prime and appropriate for an RSA prime. */
/* This has different functionality based on whether we are using key sieving or not. If not, the
   number checked to see if it is divisible by the public exponent, then the number is adjusted
   either up or down in order to make it a better candidate. It is then checked for being probably
   prime. */
/* If sieving is used, the number is used to root a sieving process. */
TPM_RC
RsaCheckPrime(
	      bigNum           prime,
	      UINT32           exponent,
	      RAND_STATE      *rand
	      )
{
#if !RSA_KEY_SIEVE
    TPM_RC          retVal = TPM_RC_SUCCESS;
    UINT32          modE = BnModWord(prime, exponent);
    NOT_REFERENCED(rand);
    if(modE == 0)
	// evenly divisible so add two keeping the number odd
	BnAddWord(prime, prime, 2);
    // want 0 != (p - 1) mod e
    // which is 1 != p mod e
    else if(modE == 1)
	// subtract 2 keeping number odd and insuring that
	// 0 != (p - 1) mod e
	BnSubWord(prime, prime, 2);
    if(BnIsProbablyPrime(prime, rand) == 0)
	ERROR_RETURN(g_inFailureMode ? TPM_RC_FAILURE : TPM_RC_VALUE);
 Exit:
    return retVal;
#else
    return PrimeSelectWithSieve(prime, exponent, rand);
#endif
}
/*********************** temporary ********************/
#include <openssl/bn.h>
#include "TpmToOsslMath_fp.h"
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
struct bignum_st
       {
       BN_ULONG *d;
       int top;
       int dmax;
       int neg;
       int flags;
       };
#endif
static
void BIGNUM_print(
		  const char      *label,
		  const BIGNUM    *a,
		  BOOL             eol
		  )
{
    BN_ULONG        *d;
    int              i;
    int              notZero = FALSE;
    if(label != NULL)
	printf("%s", label);
    if(a == NULL)
	{
	    printf("NULL");
	    goto done;
	}
    if (a->neg)
	printf("-");
    i = a->top;
    d = &a->d[i - 1];
    for(i = a->top; i > 0; i--)
	{
	    int         j;
	    BN_ULONG    l = *d--;
	    for(j = BN_BITS2 - 8; j >= 0; j -= 8)
		{
		    BYTE    b = (BYTE)((l >> j) & 0xFF);
		    notZero = notZero || (b != 0);
		    if(notZero)
			printf("%02x", b);
		}
	    if(!notZero)
		printf("0");
	}
 done:
    if(eol)
	printf("\n");
    return;
}
/*********************** temporary ********************/
/*
 * RsaAdjustPrimeCandidate_Old is the pre-rev.155 algorithm used; we
 * still have to use it for old seeds to maintain backwards compatibility.
 */
static void
RsaAdjustPrimeCandidate_Old(
                            bigNum      prime
                           )
{
   UINT16  highBytes;
    crypt_uword_t       *msw = &prime->d[prime->size - 1];
#define MASK (MAX_CRYPT_UWORD >> (RADIX_BITS - 16))
    highBytes = *msw >> (RADIX_BITS - 16);
    // This is fixed point arithmetic on 16-bit values
    highBytes = ((UINT32)highBytes * (UINT32)0x4AFB) >> 16;
    highBytes += 0xB505;
    *msw = ((crypt_uword_t)(highBytes) << (RADIX_BITS - 16)) + (*msw & MASK);
    prime->d[0] |= 1;
}
/* 10.2.16.1.7 AdjustPrimeCandiate() */
/* This function adjusts the candidate prime so that it is odd and > root(2)/2. This allows the
   product of these two numbers to be .5, which, in fixed point notation means that the most
   significant bit is 1. For this routine, the root(2)/2 is approximated with 0xB505 which is, in
   fixed point is 0.7071075439453125 or an error of 0.0001%. Just setting the upper two bits would
   give a value > 0.75 which is an error of > 6%. Given the amount of time all the other
   computations take, reducing the error is not much of a cost, but it isn't totally required
   either. */
/* The function also puts the number on a field boundary. */
static void
RsaAdjustPrimeCandidate_New(
			    bigNum          prime
			   )
{
#if RADIX_BITS == 64
    UINT32       msw = prime->d[prime->size - 1] >> 32;
#else
    UINT32       msw = prime->d[prime->size - 1];
#endif
    UINT32       adjusted;
fprintf(stderr, "%d: msw = 0x%08x\n", __LINE__, msw);
    // Multiplying 0xff...f by 0x4AFB gives 0xff..f - 0xB5050...0
    adjusted = (msw >> 16) * 0x4AFB;
fprintf(stderr, "%d: adjusted = 0x%08x\n", __LINE__, adjusted);
    adjusted += ((msw & 0xFFFF) * (UINT32)0x4AFB) >> 16;
fprintf(stderr, "%d: adjusted = 0x%08x\n", __LINE__, adjusted);
    adjusted += 0xB5050000UL;
fprintf(stderr, "%d: adjusted = 0x%08x\n", __LINE__, adjusted);
#if RADIX_BITS == 64
    prime->d[prime->size - 1] = (UINT64)adjusted << 32 |
                                (prime->d[prime->size - 1] & 0xffffffff);
#else
    prime->d[prime->size - 1] = adjusted;
#endif
    // make sure the number is odd
    prime->d[0] |= 1;
}
LIB_EXPORT void
RsaAdjustPrimeCandidate(
			bigNum          prime
			)
{
    BIGNUM *p;
    p = BigInitialized(prime);
    BIGNUM_print("before adjust: ", p, TRUE);
    BN_free(p);

    if (1)
        RsaAdjustPrimeCandidate_Old(prime);
    else
        RsaAdjustPrimeCandidate_New(prime);

    p = BigInitialized(prime);
    BIGNUM_print(" after adjust: ", p, TRUE);
    BN_free(p);
}
/* 10.2.16.1.8 BnGeneratePrimeForRSA() */
/* Function to generate a prime of the desired size with the proper attributes for an RSA prime. */
TPM_RC
BnGeneratePrimeForRSA(
		      bigNum          prime,
		      UINT32          bits,
		      UINT32          exponent,
		      RAND_STATE      *rand
		      )
{
    BOOL            found = FALSE;
    //
    // Make sure that the prime is large enough
    pAssert(prime->allocated >= BITS_TO_CRYPT_WORDS(bits));
    // Only try to handle specific sizes of keys in order to save overhead
    pAssert((bits % 32) == 0);
    
    prime->size = BITS_TO_CRYPT_WORDS(bits);
    
    while(!found)
	{
	    if (1) {
		DRBG_Generate(rand, (BYTE *)prime->d, (UINT16)BITS_TO_BYTES(bits));
	    } else {
		    TPM2B_TYPE(LARGEST, LARGEST_NUMBER + 8);
		    TPM2B_LARGEST    large;
		    large.b.size = (UINT16)BITS_TO_BYTES(bits);
		    if (DRBG_Generate(rand, large.t.buffer, large.t.size) == large.t.size)
			{
			    BnFrom2B(prime, &large.b);
			}
		    else
			{
			    return TPM_RC_FAILURE;
			}
	    }
	    if(g_inFailureMode)
		return TPM_RC_FAILURE;
	    RsaAdjustPrimeCandidate(prime);
	    found = RsaCheckPrime(prime, exponent, rand) == TPM_RC_SUCCESS;
	}
    return TPM_RC_SUCCESS;
}
#endif // TPM_ALG_RSA
