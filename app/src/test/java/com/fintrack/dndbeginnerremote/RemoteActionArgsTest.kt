package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** v2.36 #91: numbers in a remote player's action are read in 64 bits and clamped (host side). */
class RemoteActionArgsTest {
    @Test
    fun longValuesNeverWrapIntoAValidIndex() {
        // Long.toInt() used to turn these into 1 / 0 / a negative number.
        assertEquals(-1, RemoteActionArgs.index(4_294_967_297L))
        assertEquals(-1, RemoteActionArgs.index(4_294_967_296L))
        assertEquals(-1, RemoteActionArgs.index(2_147_483_648L))
        assertEquals(-1, RemoteActionArgs.index(Long.MIN_VALUE))
        assertEquals(-1, RemoteActionArgs.index(-5L))
        assertEquals(-1, RemoteActionArgs.index(1e300))
        assertEquals(-1, RemoteActionArgs.index(64L))
        assertEquals(2, RemoteActionArgs.index(2L))
        assertEquals(3, RemoteActionArgs.index(3.0))
        assertEquals(1, RemoteActionArgs.index(" 1 "))
        assertEquals(0, RemoteActionArgs.index(null))
        assertEquals(0, RemoteActionArgs.index("abc"))
        assertEquals(0, RemoteActionArgs.index(2.5))
        assertEquals(0, RemoteActionArgs.index(Double.NaN))
        assertEquals(0, RemoteActionArgs.index(mapOf("a" to 1)))
        assertEquals(0, RemoteActionArgs.index(true))
    }

    @Test
    fun classAndStatIndexStayInRange() {
        assertEquals(0, RemoteActionArgs.classId(4_294_967_297L)) // not Wizard (1)
        assertEquals(4, RemoteActionArgs.classId(4L))
        assertEquals(0, RemoteActionArgs.classId(5L))
        assertEquals(0, RemoteActionArgs.classId("x"))
        assertEquals(5, RemoteActionArgs.statIndex(5L, -1))
        assertEquals(-1, RemoteActionArgs.statIndex(6L, -1))   // never pulled onto CHA
        assertEquals(-1, RemoteActionArgs.statIndex(4_294_967_296L, -1)) // not STR (0)
        assertEquals(2, RemoteActionArgs.statIndex(null, 2))
    }

    @Test
    fun genericIntClamps() {
        assertEquals(100, RemoteActionArgs.int(Long.MAX_VALUE, 0, 100, 7))
        assertEquals(0, RemoteActionArgs.int(Long.MIN_VALUE, 0, 100, 7))
        assertEquals(7, RemoteActionArgs.int(Double.POSITIVE_INFINITY, 0, 100, 7)) // not a whole number
        assertEquals(7, RemoteActionArgs.int("99999999999999999999999", 0, 100, 7)) // not a Long
        assertEquals(42, RemoteActionArgs.int(42, 0, 100, 7))
    }

    @Test
    fun levelUpBatchIsBounded() {
        // Used to repeat two billion JNI calls on the host.
        val huge = RemoteActionArgs.allocations("2000000000,0,0,0,0,0")
        assertEquals(RemoteActionArgs.MAX_BATCH_POINTS, huge.sum())
        assertArrayEquals(intArrayOf(2, 1, 0, 0, 3, 0), RemoteActionArgs.allocations("2,1,0,0,3,0"))
        assertArrayEquals(intArrayOf(0, 0, 0, 0, 0, 0), RemoteActionArgs.allocations("-5,abc,,1e9,-0,x"))
        assertArrayEquals(intArrayOf(1, 2, 3, 4, 5, 6), RemoteActionArgs.allocations("1,2,3,4,5,6,7,8"))
        assertEquals(RemoteActionArgs.MAX_BATCH_POINTS, RemoteActionArgs.allocations("99999999999999999999,500,500,500,500,500").sum())
        assertTrue(RemoteActionArgs.allocations("1,".repeat(500)).sum() == 0) // oversized string ignored
        assertTrue(RemoteActionArgs.allocations(null).all { it == 0 })
    }
}
