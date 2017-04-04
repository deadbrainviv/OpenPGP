#include "sign.h"

PKA::Values pka_sign(const std::string & digest, const uint8_t pka, const PKA::Values & pri, const PKA::Values & pub, const uint8_t hash, std::string & error){
    if ((pka == PKA::RSA_ENCRYPT_OR_SIGN) ||
        (pka == PKA::RSA_SIGN_ONLY)){
        // RFC 4880 sec 5.2.2
        // If RSA, hash value is encoded using EMSA-PKCS1-v1_5
        return {RSA_sign(EMSA_PKCS1_v1_5(hash, digest, bitsize(pub[0]) >> 3), pri, pub)};
    }
    else if (pka == PKA::DSA){
        return DSA_sign(digest, pri, pub);
    }

    error += "Error: Undefined or incorrect PKA number: " + std::to_string(pka) + "\n";
    return {};
}

PKA::Values pka_sign(const std::string & digest, const uint8_t pka, const PKA::Values & pri, const PKA::Values & pub, const uint8_t hash){
    std::string error;
    return pka_sign(digest, pka, pri, pub, hash, error);
}

Tag2::Ptr create_sig_packet(const uint8_t version, const uint8_t type, const uint8_t pka, const uint8_t hash, const std::string & keyid){
    // Set up signature packet
    Tag2::Ptr tag2 = std::make_shared <Tag2> ();
    tag2 -> set_version(version);
    tag2 -> set_pka(pka);
    tag2 -> set_type(type);
    tag2 -> set_hash(hash);

    // Set Time
    Tag2Sub2::Ptr tag2sub2 = std::make_shared <Tag2Sub2> ();
    tag2sub2 -> set_time(now());
    tag2 -> set_hashed_subpackets({tag2sub2});

    // Set Key ID
    Tag2Sub16::Ptr tag2sub16 = std::make_shared <Tag2Sub16> ();
    tag2sub16 -> set_keyid(keyid);
    tag2 -> set_unhashed_subpackets({tag2sub16});

    return tag2;
}

PGPDetachedSignature sign_detached_signature(const SignArgs & args, const std::string & data, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return PGPDetachedSignature();
    }

    Tag5::Ptr signer = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!signer){
        error += "Error: No Private Key for signing found.\n";
        return PGPDetachedSignature();
    }

    // Check if key has been revoked
    const int rc = check_revoked(args.pri, error);
    if (rc == 1){
        error += "Error: Key " + hexlify(signer -> get_keyid()) + " has been revoked. Nothing done.\n";
        return PGPDetachedSignature();
    }
    else if (rc == 1){
        error += "Error: check_revoked failed.\n";
        return PGPDetachedSignature();
    }

    // create Signature Packet
    Tag2::Ptr sig = create_sig_packet(args.version, Signature_Type::SIGNATURE_OF_A_BINARY_DOCUMENT, signer -> get_pka(), args.hash, signer -> get_keyid());
    const std::string digest = to_sign_00(binary_to_canonical(data), sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer -> get_pka(), signer -> decrypt_secret_keys(args.passphrase), signer -> get_mpi(), args.hash, error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return PGPDetachedSignature();
    }
    sig -> set_mpi(vals);

    PGPDetachedSignature signature;
    signature.set_keys({std::make_pair("Version", "cc")});
    signature.set_packets({sig});

    return signature;
}

PGPDetachedSignature sign_detached_signature(const SignArgs & args, const std::string & data){
    std::string error;
    return sign_detached_signature(args, data, error);
}

// 0x00: Signature of a binary document.
PGPMessage sign_binary(const SignArgs & args, const std::string & filename, const std::string & data, const uint8_t compress, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return PGPDetachedSignature();
    }

    // find signing key
    Tag5::Ptr signer = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!signer){
        error += "Error: No signing key found.\n";
        return PGPMessage();
    }

    // create One-Pass Signature Packet
    Tag4::Ptr tag4 = std::make_shared <Tag4> ();
    tag4 -> set_type(0);
    tag4 -> set_hash(args.hash);
    tag4 -> set_pka(signer -> get_pka());
    tag4 -> set_keyid(signer -> get_keyid());
    tag4 -> set_nested(1); // 1 for no nesting

    // put source data into Literal Data Packet
    Tag11::Ptr tag11 = std::make_shared <Tag11> ();
    tag11 -> set_format('b');
    tag11 -> set_filename(filename);
    tag11 -> set_time(now());
    tag11 -> set_literal(data);

    // sign data
    Tag2::Ptr sig = create_sig_packet(args.version, Signature_Type::SIGNATURE_OF_A_BINARY_DOCUMENT, signer -> get_pka(), args.hash, signer -> get_keyid());
    const std::string digest = to_sign_00(binary_to_canonical(tag11 -> get_literal()), sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer -> get_pka(), signer -> decrypt_secret_keys(args.passphrase), signer -> get_mpi(), args.hash, error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return PGPMessage();
    }
    sig -> set_mpi(vals);

    // put everything together
    PGPMessage signature;
    signature.set_keys({std::make_pair("Version", "cc")});
    signature.set_packets({tag4, tag11, sig});

    if (compress){ // only use a Compressed Data Packet if compression was used; don't bother for uncompressed data
        Tag8 tag8;
        tag8.set_data(signature.raw());
        tag8.set_comp(compress);
        std::string raw = tag8.write(Packet::Format::NEW);
        signature = PGPMessage(raw);
    }

    return signature;
}

PGPMessage sign_binary(const SignArgs & args, const std::string & filename, const std::string & data, const uint8_t compress){
    std::string error;
    return sign_binary(args, filename, data, compress, error);
}

// 0x01: Signature of a canonical text document.
PGPCleartextSignature sign_cleartext_signature(const SignArgs & args, const std::string & text, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad argument.\n";
        return PGPCleartextSignature();
    }

    // find signing key
    Tag5::Ptr signer = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!signer){
        error += "Error: No signing key found.\n";
        return PGPCleartextSignature();
    }

    // create signature
    Tag2::Ptr sig = create_sig_packet(args.version, Signature_Type::SIGNATURE_OF_A_CANONICAL_TEXT_DOCUMENT, signer -> get_pka(), args.hash, signer -> get_keyid());
    const std::string digest = to_sign_01(PGPCleartextSignature::data_to_text(text), sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer -> get_pka(), signer -> decrypt_secret_keys(args.passphrase), signer -> get_mpi(), args.hash, error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return PGPCleartextSignature();
    }
    sig -> set_mpi(vals);

    // put signature into Detached Signature
    PGPDetachedSignature signature;
    signature.set_keys({std::make_pair("Version", "cc")});
    signature.set_packets({sig});

    // put signature under cleartext
    PGPCleartextSignature message;
    message.set_hash_armor_header({std::make_pair("Hash", Hash::NAME.at(args.hash))});
    message.set_message(text);
    message.set_sig(signature);

    return message;
}

PGPCleartextSignature sign_cleartext_signature(const SignArgs & args, const std::string & text){
    std::string error;
    return sign_cleartext_signature(args, text, error);
}

// 0x02: Standalone signature.
PGPDetachedSignature sign_standalone_signature(const SignArgs & args, const Tag2::Ptr & src, const uint8_t compress, std::string & error){
    throw std::runtime_error("Error: sign_standalone_signature not implemented yet");
    return PGPDetachedSignature();
}

PGPDetachedSignature sign_standalone_signature(const SignArgs & args, const Tag2::Ptr & src, const uint8_t compress){
    std::string error;
    return sign_standalone_signature(args, src, compress, error);
}

// 0x10: Generic certification of a User ID and Public-Key packet.
// 0x11: Persona certification of a User ID and Public-Key packet.
// 0x12: Casual certification of a User ID and Public-Key packet.
// 0x13: Positive certification of a User ID and Public-Key packet.
Tag2::Ptr sign_primary_key(const Tag5::Ptr signer_signing_key, const std::string & passphrase, const Key::Ptr & signee_primary_key, const User::Ptr & signee_id, Tag2::Ptr & sig, std::string & error){
    if (!signer_signing_key){
        error += "Error: No signing key given.\n";
        return nullptr;
    }

    if (!signee_primary_key){
        error += "Error: No signee primary key given.\n";
        return nullptr;
    }

    if (!Packet::is_primary_key(signee_primary_key -> get_tag())){
        error += "Error: signee key is not a primary key.\n";
        return nullptr;
    }

    if (!signee_id){
        error += "Error: No User Identifier given.\n";
        return nullptr;
    }

    if (!sig){
        error += "Error: No signature data given.\n";
        return nullptr;
    }

    if (!Signature_Type::is_certification(sig -> get_type())){
        error += "Error: Invalid Certification Value: 0x" + makehex(sig -> get_type(), 2) + "\n";
        return nullptr;
    }

    const std::string digest = to_sign_cert(sig -> get_type(), signee_primary_key, signee_id, sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer_signing_key -> get_pka(), signer_signing_key -> decrypt_secret_keys(passphrase), signer_signing_key -> get_mpi(), sig -> get_hash(), error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return nullptr;
    }
    sig -> set_mpi(vals);

    return sig;
}

PGPPublicKey sign_primary_key(const SignArgs & args, const PGPPublicKey & signee, const std::string & user, const uint8_t cert, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad arguments.\n";
        return PGPPublicKey();
    }

    // check if signer has already been revoked
    if (check_revoked(args.pri, error)){
        error += "Error: Signer key is revoked. Nothing done.\n";
        return PGPPublicKey();
    }

    if (!signee.meaningful(error)){
        error += "Error: Bad signee key.\n";
        return PGPPublicKey();
    }

    if (check_revoked(signee, error)){
        error += "Error: Signer key is revoked. Nothing done.\n";
        return PGPPublicKey();
    }

    if (!Signature_Type::is_certification(cert)){
        error += "Error: Invalid Certification Value: 0x" + makehex(cert, 2) + "\n";
        return PGPPublicKey();
    }

    // get signer's signing packet
    Tag5::Ptr signer_signing_key = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!signer_signing_key){
        error += "Error: Signing key not found.\n";
        return PGPPublicKey();
    }

    const PGP::Packets & signee_packets = signee.get_packets();
    Key::Ptr signee_primary_key = std::static_pointer_cast <Key> (signee_packets[0]);
    User::Ptr signee_id = nullptr;

    // find matching user identifier
    PGP::Packets::size_type i = 1;
    do{
        // find matching user identifier
        if (Packet::is_user(signee_packets[i] -> get_tag())){
            // if the packet is a User ID
            if (signee_packets[i] -> get_tag() == Packet::USER_ID){
                Tag13::Ptr tag13 = std::static_pointer_cast <Tag13> (signee_packets[i]);
                if (tag13 -> get_contents().find(user) != std::string::npos){
                    signee_id = tag13;
                    i++; // go past User ID packet
                    break;
                }
            }
            // else if (signee_packets[i] -> get_tag() == Packet::USER_ATTRIBUTE){}
        }

        i++;
    } while (i < signee_packets.size());

    if (!signee_id){
        error += "Error: No Signee user ID found.\n";
        return PGPPublicKey();
    }

    // search through signatures to see signer has already certified this user
    while (i < signee_packets.size() && (signee_packets[i] -> get_tag() == Packet::SIGNATURE)){
        const int rc = verify_key(signer_signing_key, signee_primary_key, signee_id, std::static_pointer_cast <Tag2> (signee_packets[i]), error);
        if (rc == -1){
            error += "Error: Signature verification failure.\n";
            return PGPPublicKey();
        }
        else if (rc == 1){
            std::cerr << "Warning: Primary Key and User ID have already been signed by " << args.pri << std::endl;
            return signee;
        }

        i++;
    }

    // sign key
    Tag2::Ptr sig = create_sig_packet(args.version, cert, signer_signing_key -> get_pka(), args.hash, signer_signing_key -> get_keyid());
    if (!sig){
        error += "Error: Signature packet generation failure.\n";
        return PGPPublicKey();
    }

    sig = sign_primary_key(signer_signing_key, args.passphrase, signee_primary_key, signee_id, sig, error);
    if (!sig){
        error += "Error: Signature calculation failure.\n";
        return PGPPublicKey();
    }

    // Create output key
    PGPPublicKey out(signee);
    PGP::Packets out_packets;

    // push all packets up to and including out packet into new packets
    PGP::Packets::size_type j;
    for(j = 0; j < (signee_packets.size()) && (j < i); j++){
        out_packets.push_back(signee_packets[j]);
    }

    // append signature to signatures following key
    out_packets.push_back(sig);

    // append rest of packets
    while (j < signee_packets.size()){
        out_packets.push_back(signee_packets[j++]);
    }

    out.set_packets(out_packets);

    return out;
}

PGPPublicKey sign_primary_key(const SignArgs & args, const PGPPublicKey & signee, const std::string & user, const uint8_t cert){
    std::string error;
    return sign_primary_key(args, signee, user, cert, error);
}

// 0x18: Subkey Binding Signature
Tag2::Ptr sign_subkey_binding(const Tag5::Ptr & primary, const std::string & passphrase, const Tag7::Ptr & sub, Tag2::Ptr & sig, std::string & error){
    if (!primary){
        error += "Error: No primary key.\n";
        return nullptr;
    }

    if (!sub){
        error += "Error: No subkey.\n";
        return nullptr;
    }

    if (!sig){
        error += "Error: No signature.\n";
        return nullptr;
    }

    const std::string digest = to_sign_18(primary, sub, sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, primary -> get_pka(), primary -> decrypt_secret_keys(passphrase), primary -> get_mpi(), sig -> get_hash(), error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return nullptr;
    }
    sig -> set_mpi(vals);

    return sig;
}

// 0x19: Primary Key Binding Signature
Tag2::Ptr sign_primary_key_binding(const SignArgs & args, const PGPPublicKey & signee, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad arguments.\n";
        return nullptr;
    }

    // find signing subkey
    Tag5::Ptr subkey = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!subkey){
        error += "Error: No Signing Subkey found.\n";
        return nullptr;
    }

    // move subkey data into subkey packet
    Tag7::Ptr signer_subkey = std::static_pointer_cast <Tag7> (subkey);

    // get signee primary and subkey
    Tag6::Ptr signee_primary = nullptr;
    for(Packet::Ptr const & p : args.pri.get_packets()){
        if (p -> get_tag() == Packet::PUBLIC_KEY){
            signee_primary = std::static_pointer_cast <Tag6> (p);
            break;
        }
    }

    if (!signee_primary){
        error += "Error: Signee Primary Key not found.\n";
        return nullptr;
    }

    Tag14::Ptr signee_subkey = nullptr;
    for(Packet::Ptr const & p : args.pri.get_packets()){
        if (p -> get_tag() == Packet::PUBLIC_SUBKEY){
            signee_subkey = std::static_pointer_cast <Tag14> (p);
            break;
        }
    }

    if (!signee_subkey){
        error += "Error: Singee Subkey not found.\n";
        return nullptr;
    }

    Tag2::Ptr sig = create_sig_packet(args.version, Signature_Type::PRIMARY_KEY_BINDING_SIGNATURE, signer_subkey -> get_pka(), args.hash, signer_subkey -> get_keyid());
    if (!sig){
        error += "Error: Signature packet generation failure.\n";
        return nullptr;
    }

    const std::string digest = to_sign_18(signee_primary, signer_subkey, sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer_subkey -> get_pka(), signer_subkey -> decrypt_secret_keys(args.passphrase), signer_subkey -> get_mpi(), args.hash, error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return nullptr;
    }
    sig -> set_mpi(vals);

    return sig;
}

Tag2::Ptr sign_primary_key_binding(const SignArgs & args, const PGPPublicKey & signee){
    std::string error;
    return sign_primary_key_binding(args, signee, error);
}

PGPDetachedSignature sign_timestamp(const SignArgs & args, const time_t time, std::string & error){
    if (!args.valid(error)){
        error += "Error: Bad arguments.\n";
        return PGPDetachedSignature();
    }
    
    Tag5::Ptr signer = std::static_pointer_cast <Tag5> (find_signing_key(args.pri));
    if (!signer){
        error += "Error: Signing key not found.\n";
        return PGPDetachedSignature();
    }
    
    Tag2::Ptr sig = create_sig_packet(args.version, Signature_Type::TIMESTAMP_SIGNATURE, signer -> get_pka(), args.hash, signer -> get_keyid());
    if (!sig){
        error += "Error: Signature packet generation failure.\n";
        return PGPDetachedSignature();
    }

    Tag2Sub2::Ptr tag2sub2 = std::make_shared <Tag2Sub2> ();
    tag2sub2 -> set_time(time);
    sig -> set_hashed_subpackets({tag2sub2});

    const std::string digest = to_sign_40(sig);
    sig -> set_left16(digest.substr(0, 2));
    PKA::Values vals = pka_sign(digest, signer -> get_pka(), signer -> decrypt_secret_keys(args.passphrase), signer -> get_mpi(), args.hash, error);
    if (!vals.size()){
        error += "Error: PKA Signing failed.\n";
        return PGPDetachedSignature();
    }
    sig -> set_mpi(vals);
    
    PGPDetachedSignature timestamp;
    timestamp.set_keys({std::make_pair("Version", "cc")});
    timestamp.set_packets({sig});

    return timestamp;
}
