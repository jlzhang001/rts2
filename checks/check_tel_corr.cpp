#include "gemtest.h"
#include "altaztest.h"
#include "check_utils.h"

#include <time.h>
#include <stdlib.h>
#include <check.h>
#include <libnova/libnova.h>

// global telescope object
GemTest* gemTest;
AltAzTest* altazTest1850;	// 1850 meters above see level
AltAzTest* altazTest1680;	// 1680 meters above see level, to check for pressure effects
AltAzTest* altazTestNutation;	// test if nutation calculation remains the same..
GemTest* gemTest4000;		// 4 atmosphere pressure

void setup_tel (void)
{
	static const char *argv[] = {"testapp"};

	gemTest = new GemTest(0, (char **) argv);
	gemTest->setTelescope (20.70752, -156.257, 3039, 67108864, 67108864, 0, 75.81458333, -5.8187805555, 186413.511111, 186413.511111, -81949557, -47392062, -76983817, -21692458);

	altazTest1850 = new AltAzTest(0, (char **) argv);
	altazTest1850->setTelescope (-32.5, 20.2, 1850, 67108864, 67108864, 90, -7, 186413.511111, 186413.511111, -80000000, 80000000, -40000000, 40000000);

	altazTest1680 = new AltAzTest(0, (char **) argv);
	altazTest1680->setTelescope (-32.5, 20.2, 1680, 67108864, 67108864, 90, -7, 186413.511111, 186413.511111, -80000000, 80000000, -40000000, 40000000);

	// telescope at 4 atmosphere pressure - test for pressure and altitude missuse

	gemTest4000 = new GemTest(0, (char **) argv);
	gemTest4000->setTelescope (20.70752, -156.257, -13500, 67108864, 67108864, 0, 75.81458333, -5.8187805555, 186413.511111, 186413.511111, -81949557, -47392062, -76983817, -21692458);

	altazTestNutation = new AltAzTest(0, (char **) argv);
	altazTestNutation->setTelescope (-40, -5, 200, 67108864, 67108864, 90, -7, 186413.511111, 186413.511111, -80000000, 80000000, -40000000, 40000000);
}

void teardown_tel (void)
{
	delete gemTest4000;
	gemTest4000 = NULL;

	delete altazTest1850;
	altazTest1850 = NULL;

	delete altazTest1680;
	altazTest1680 = NULL;

	delete altazTestNutation;
	altazTestNutation = NULL;

	delete gemTest;
	gemTest = NULL;
}

START_TEST(test_pressure)
{
	ck_assert_dbl_eq (gemTest4000->test_getAltitudePressure (-13500, 1010), 4084.32, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (0, 1010), 1010, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (1000, 1010), 895.86, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (2345, 1010), 759.04, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (4000, 1010), 614.43, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (4015, 998), 605.99, 10e-1);
	ck_assert_dbl_eq (gemTest->test_getAltitudePressure (6087, 998), 459.19, 10e-1);
}
END_TEST

START_TEST(test_refraction)
{
	struct ln_hrz_posn tpos, tpos_2;
	struct ln_equ_posn teq;
	double JD = 2452134;

	tpos.alt = 20;
	tpos.az = 300;
	gemTest->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 43.8965, 10e-4);
	ck_assert_dbl_eq (teq.dec, -18.5755, 10e-4);

	gemTest->test_getHrzFromEqu (&teq, JD, &tpos_2);

	ck_assert_dbl_eq (tpos_2.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos_2.alt, 20.0000, 10e-4);

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 43.8687, 10e-4);
	ck_assert_dbl_eq (teq.dec, -18.5595, 10e-4);

	gemTest->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 20.0308, 10e-4);

	teq.ra += 1;

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 44.8395, 10e-4);
	ck_assert_dbl_eq (teq.dec, -18.5430, 10e-4);

	gemTest->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 299.4803, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 19.2508, 10e-4);

	tpos.alt = 89;
	tpos.az = 231;
	gemTest->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 345.5796, 10e-4);
	ck_assert_dbl_eq (teq.dec, 21.3348, 10e-4);

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 345.5796, 10e-4);
	ck_assert_dbl_eq (teq.dec, 21.3348, 10e-4);

	gemTest->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 231.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 89.0000, 10e-4);

	tpos.alt = 2.34;
	tpos.az = 130.558;
	gemTest->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 240.5846, 10e-4);
	ck_assert_dbl_eq (teq.dec, 38.4727, 10e-4);

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 240.8070, 10e-4);
	ck_assert_dbl_eq (teq.dec, 38.5529, 10e-4);

	gemTest->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 130.5580, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 2.5315, 10e-4);
}
END_TEST

START_TEST(test_refraction1850)
{
	struct ln_hrz_posn tpos;
	struct ln_equ_posn teq;
	double JD = 2452134;

	ck_assert_dbl_eq (altazTest1850->getPressure (), 807.27, 10e-1);

	tpos.alt = 20;
	tpos.az = 300;
	altazTest1850->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 248.6688, 10e-4);
	ck_assert_dbl_eq (teq.dec, -35.4527, 10e-4);

	altazTest1850->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 248.6293, 10e-4);
	ck_assert_dbl_eq (teq.dec, -35.4686, 10e-4);

	altazTest1850->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 20.0358, 10e-5);

	teq.ra += 1;

	altazTest1850->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 249.5885, 10e-4);
	ck_assert_dbl_eq (teq.dec, -35.4853, 10e-4);

	altazTest1850->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.3856, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 19.3444, 10e-4);

	tpos.alt = 40;
	tpos.az = 300;
	altazTest1850->test_getEquFromHrz (&tpos, JD, &teq);

	altazTest1850->test_applyRefraction (&teq, JD, false);

	altazTest1850->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 40.0155, 10e-5);

	tpos.alt = 89;
	tpos.az = 231;
	altazTest1850->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 162.1174, 10e-4);
	ck_assert_dbl_eq (teq.dec, -31.8673, 10e-4);

	altazTest1850->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 162.1174, 10e-4);
	ck_assert_dbl_eq (teq.dec, -31.8673, 10e-4);

	altazTest1850->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 231.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 89.0000, 10e-4);

	tpos.alt = 2.34;
	tpos.az = 130.558;
	altazTest1850->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 98.0053, 10e-4);
	ck_assert_dbl_eq (teq.dec, 31.7351, 10e-4);

	altazTest1850->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 98.2020, 10e-4);
	ck_assert_dbl_eq (teq.dec, 31.5887, 10e-4);

	altazTest1850->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 130.5580, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 2.5623, 10e-4);
}
END_TEST

START_TEST(test_refraction1680)
{
	struct ln_hrz_posn tpos;
	struct ln_equ_posn teq;
	double JD = 2452134;

	ck_assert_dbl_eq (altazTest1680->getPressure (), 824.39, 10e-1);

	tpos.alt = 20;
	tpos.az = 300;
	altazTest1680->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 248.6688, 10e-4);
	ck_assert_dbl_eq (teq.dec, -35.4527, 10e-4);

	altazTest1680->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 248.6293, 10e-4);
	ck_assert_dbl_eq (teq.dec, -35.4686, 10e-4);

	altazTest1680->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 20.0365, 10e-5);

	tpos.alt = 40;
	tpos.az = 300;
	altazTest1680->test_getEquFromHrz (&tpos, JD, &teq);

	altazTest1680->test_applyRefraction (&teq, JD, false);

	altazTest1680->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 40.0159, 10e-5);
}
END_TEST

START_TEST(test_nutation)
{
	struct ln_equ_posn pos;
	struct ln_hrz_posn hrz;
	int32_t azc = -20000000;
	int32_t altc = 1000;
	double JD = 2452164;
	int ret;

	pos.ra = 270;
	pos.dec = -85;

	altazTestNutation->setCorrections (true, true, true, true);

	ret = altazTestNutation->test_sky2counts (JD, &pos, azc, altc);
	ck_assert_int_eq (ret, 0);
	ck_assert_int_eq (azc, -17930975);
	ck_assert_int_eq (altc, 10886604);

	// check meridian calculations
	hrz.az = 180;
	hrz.alt = 50;

	double ST = ln_get_apparent_sidereal_time (JD);

	altazTestNutation->test_getEquFromHrz (&hrz, JD, &pos);
	ck_assert_dbl_eq (pos.ra, ST * 15.0 + altazTestNutation->getLongitude (), 10e-10);

	altazTestNutation->test_getHrzFromEquST (&pos, ST, &hrz);
	ck_assert_dbl_eq (hrz.az, 180, 10e-10);

	pos.ra = ST * 15.0 + altazTestNutation->getLongitude ();
	pos.dec = -30;

	altazTestNutation->test_getHrzFromEquST (&pos, ST, &hrz);
	ck_assert_dbl_eq (hrz.az, 180, 10e-10);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, ST, &hrz);
	if (hrz.az >= 360.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az, 0, 10e-10);

	pos.dec = altazTestNutation->getLatitude () + altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, ST, &hrz);
	if (hrz.az >= 360.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az, 0, 10e-10);

	double MST = ln_get_mean_sidereal_time (JD);

	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, -18.4364, 10e-3);

	// the biggest nutation period is 180 days, so adding 90 days should change the sign around mean..
	JD += 90;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, -19.3717, 10e-3);

	JD += 90;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, -17.5269, 10e-3);

	JD += 90;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, -18.9762, 10e-3);

	JD += 11 * 365;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, 12.6886, 10e-3);

	JD += 365;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, 7.7707, 10e-3);

	JD += 365;

	ST = ln_get_apparent_sidereal_time (JD);
	MST = ln_get_mean_sidereal_time (JD);

	pos.ra = ST * 15.0 + 180 + altazTestNutation->getLongitude ();
	pos.dec = altazTestNutation->getLatitude () - altazTestNutation->getLatitude () / 2.0;

	altazTestNutation->test_getHrzFromEquST (&pos, MST, &hrz);
	if (hrz.az >= 180.0)
		hrz.az -= 360.0;
	ck_assert_dbl_eq (hrz.az * 3600.0, 1.6733, 10e-3);
}
END_TEST

START_TEST(test_refraction4000)
{
	struct ln_hrz_posn tpos;
	struct ln_equ_posn teq;
	double JD = 2452134;

	tpos.alt = 40;
	tpos.az = 300;
	gemTest4000->test_getEquFromHrz (&tpos, JD, &teq);

	ck_assert_dbl_eq (teq.ra, 26.7491, 10e-4);
	ck_assert_dbl_eq (teq.dec, -7.5267, 10e-4);

	gemTest4000->test_applyRefraction (&teq, JD, false);

	gemTest4000->test_getHrzFromEqu (&teq, JD, &tpos);

	ck_assert_dbl_eq (tpos.az, 300.0000, 10e-4);
	ck_assert_dbl_eq (tpos.alt, 40.0789, 10e-4);

	teq.ra = 27.7491;
	teq.dec = -7.5267;

	gemTest4000->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 27.6816, 10e-4);
	ck_assert_dbl_eq (teq.dec, -7.4805, 10e-4);

	teq.ra = 27.7491;
	teq.dec = -7.5267;

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 27.7376, 10e-4);
	ck_assert_dbl_eq (teq.dec, -7.5188, 10e-4);

	teq.ra = 30.7491;
	teq.dec = -7.5267;

	gemTest4000->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 30.6738, 10e-4);
	ck_assert_dbl_eq (teq.dec, -7.4784, 10e-4);

	teq.ra = 30.7491;
	teq.dec = -7.5267;

	gemTest->test_applyRefraction (&teq, JD, false);

	ck_assert_dbl_eq (teq.ra, 30.7362, 10e-4);
	ck_assert_dbl_eq (teq.dec, -7.5184, 10e-4);
}
END_TEST

Suite * tel_suite (void)
{
	Suite *s;
	TCase *tc_tel_corrs;

	s = suite_create ("Corrections");
	tc_tel_corrs = tcase_create ("Basic");

	tcase_add_checked_fixture (tc_tel_corrs, setup_tel, teardown_tel);
	tcase_add_test (tc_tel_corrs, test_pressure);
	tcase_add_test (tc_tel_corrs, test_refraction);
	tcase_add_test (tc_tel_corrs, test_refraction1850);
	tcase_add_test (tc_tel_corrs, test_refraction1680);
	tcase_add_test (tc_tel_corrs, test_nutation);
	tcase_add_test (tc_tel_corrs, test_refraction4000);
	suite_add_tcase (s, tc_tel_corrs);

	return s;
}

int main (void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = tel_suite ();
	sr = srunner_create (s);
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
