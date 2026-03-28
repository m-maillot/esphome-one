"""
On.e Bluetooth Authentication Algorithm Implementation
======================================================
Reverse-engineered from com.acis.oneapp v2.6.3 Hermes bytecode.

This implements the authorisationProcess that computes the 16-byte
encrypted response from a nonce, using the shared key and private key.
"""

from Crypto.Cipher import AES
import binascii


# Hardcoded AES-128-ECB key (same for all devices)
PRIVATE_KEY_HEX = "1141a80537444a6a85888d84115f2811"


def compute_auth_response(shared_key_hex: str, nonce_ble_bytes: bytes) -> bytes:
    """
    Compute the 16-byte authentication response.

    Args:
        shared_key_hex: The shared key as hex string (e.g. "0123456789abcdef"),
                        stored from the association process.
        nonce_ble_bytes: The raw bytes read from BLE characteristic FBDE0001
                         (before base64 decoding - this should be the raw bytes).

    Returns:
        The response bytes to write to FBDE0003 (in BLE wire format, reversed).
    """
    # Step 1: Reverse nonce bytes (BLE little-endian -> big-endian)
    nonce_reversed = bytes(reversed(nonce_ble_bytes))

    # Step 2: Convert nonce to hex string
    nonce_hex = nonce_reversed.hex()

    # Step 3: Concatenate shared key + nonce as hex strings
    plaintext_hex = shared_key_hex + nonce_hex
    print(f"  Shared key (hex):   {shared_key_hex}")
    print(f"  Nonce (hex, rev):   {nonce_hex}")
    print(f"  Plaintext (hex):    {plaintext_hex}")

    # Step 4: Convert plaintext hex to bytes
    plaintext_bytes = bytes.fromhex(plaintext_hex)
    print(f"  Plaintext (bytes):  {plaintext_bytes.hex()}")
    print(f"  Plaintext length:   {len(plaintext_bytes)} bytes")

    # Step 5: AES-128-ECB encrypt with PRIVATE_KEY
    key_bytes = bytes.fromhex(PRIVATE_KEY_HEX)
    cipher = AES.new(key_bytes, AES.MODE_ECB)
    encrypted_bytes = cipher.encrypt(plaintext_bytes)
    print(f"  Encrypted (hex):    {encrypted_bytes.hex()}")

    # Step 6: Reverse encrypted bytes (big-endian -> BLE little-endian)
    response_bytes = bytes(reversed(encrypted_bytes))
    print(f"  Response (hex, LE): {response_bytes.hex()}")

    return response_bytes


def simulate_full_auth():
    """Simulate the full authentication with example data."""
    print("=" * 60)
    print("On.e Bluetooth Authentication Simulation")
    print("=" * 60)

    # Example shared key from simulator data
    shared_key = "0123456789abcdef"  # remplacer par votre sharedKey

    # Example nonce (8 bytes as read from BLE, in little-endian wire format)
    # In practice, this comes from reading FBDE0001
    example_nonce_ble = bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08])

    print(f"\nAES Key (PRIVATE_KEY): {PRIVATE_KEY_HEX}")
    print(f"AES Key bytes: {bytes.fromhex(PRIVATE_KEY_HEX).hex()}")
    print(f"\nShared Key: {shared_key}")
    print(f"Nonce (BLE wire): {example_nonce_ble.hex()}")
    print()

    response = compute_auth_response(shared_key, example_nonce_ble)

    print(f"\nFinal response to write to FBDE0003: {response.hex()}")
    print(f"As base64: {binascii.b2a_base64(response).decode().strip()}")
    print()

    # Verify: decrypt and check
    key_bytes = bytes.fromhex(PRIVATE_KEY_HEX)
    cipher = AES.new(key_bytes, AES.MODE_ECB)
    decrypted = cipher.decrypt(bytes(reversed(response)))
    print(f"Verification (decrypt response):")
    print(f"  Decrypted: {decrypted.hex()}")
    print(f"  Should be: {shared_key}{bytes(reversed(example_nonce_ble)).hex()}")


def ble_auth_flow_pseudocode():
    """
    Full BLE authentication flow as pseudocode with actual function names.
    """
    print("""
FULL BLE AUTHENTICATION FLOW
=============================

1. connectProcess:
   - bleManager.connect(deviceId)
   - device.discoverAllServicesAndCharacteristics()
   - device.requestMTU(512)

2. associationProcess (only first time):
   - Read FBDE0002 from FBDE0000 service
   - rawBytes = base64Decode(characteristic.value)
   - sharedKey = bytesToHex(reverse(rawBytes))
   - Store sharedKey in device model

3. authorisationProcess:
   - Read FBDE0001 from FBDE0000 service (nonce)
   - nonceRaw = base64Decode(characteristic.value)
   - nonceHex = bytesToHex(reverse(nonceRaw))
   - sharedKeyHex = stored sharedKey
   - plaintext = sharedKeyHex + nonceHex (string concatenation)
   - keyBytes = hexToBytes("1141a80537444a6a85888d84115f2811")
   - aes = new AES_ECB(keyBytes)
   - encrypted = aes.encrypt(hexToBytes(plaintext))
   - response = Buffer(reverse(encrypted))
   - Write response.toBase64() to FBDE0003 of FBDE0000 service

4. identificationProcess:
   - Read 2A24 from 180A (model number)
   - Read 2A25 from 180A (serial number)
   - Read 2A26 from 180A (firmware version)

5. syncRTCProcess:
   - Write current date/time to 2A08 of 1805

6. utilisationProcess:
   - Normal data exchange via FBDE01xx, FBDE02xx, FBDE03xx characteristics
""")


if __name__ == "__main__":
    simulate_full_auth()
    ble_auth_flow_pseudocode()
