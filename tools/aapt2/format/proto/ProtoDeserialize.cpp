/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "format/proto/ProtoDeserialize.h"

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "androidfw/ResourceTypes.h"

#include "Locale.h"
#include "ResourceTable.h"
#include "ResourceUtils.h"
#include "ValueVisitor.h"

using ::android::ResStringPool;

namespace aapt {

namespace {

class ReferenceIdToNameVisitor : public DescendingValueVisitor {
 public:
  using DescendingValueVisitor::Visit;

  explicit ReferenceIdToNameVisitor(const std::map<ResourceId, ResourceNameRef>* mapping)
      : mapping_(mapping) {
    CHECK(mapping_ != nullptr);
  }

  void Visit(Reference* reference) override {
    if (!reference->id || !reference->id.value().is_valid()) {
      return;
    }

    ResourceId id = reference->id.value();
    auto cache_iter = mapping_->find(id);
    if (cache_iter != mapping_->end()) {
      reference->name = cache_iter->second.ToResourceName();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReferenceIdToNameVisitor);

  const std::map<ResourceId, ResourceNameRef>* mapping_;
};

}  // namespace

bool DeserializeConfigFromPb(const pb::Configuration& pb_config, ConfigDescription* out_config,
                             std::string* out_error) {
  out_config->mcc = static_cast<uint16_t>(pb_config.mcc());
  out_config->mnc = static_cast<uint16_t>(pb_config.mnc());

  if (!pb_config.locale().empty()) {
    LocaleValue lv;
    if (!lv.InitFromBcp47Tag(pb_config.locale())) {
      std::ostringstream error;
      error << "configuration has invalid locale '" << pb_config.locale() << "'";
      *out_error = error.str();
      return false;
    }
    lv.WriteTo(out_config);
  }

  switch (pb_config.layout_direction()) {
    case pb::Configuration_LayoutDirection_LAYOUT_DIRECTION_LTR:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_LAYOUTDIR) |
                                 ConfigDescription::LAYOUTDIR_LTR;
      break;

    case pb::Configuration_LayoutDirection_LAYOUT_DIRECTION_RTL:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_LAYOUTDIR) |
                                 ConfigDescription::LAYOUTDIR_RTL;
      break;

    default:
      break;
  }

  out_config->smallestScreenWidthDp = static_cast<uint16_t>(pb_config.smallest_screen_width_dp());
  out_config->screenWidthDp = static_cast<uint16_t>(pb_config.screen_width_dp());
  out_config->screenHeightDp = static_cast<uint16_t>(pb_config.screen_height_dp());

  switch (pb_config.screen_layout_size()) {
    case pb::Configuration_ScreenLayoutSize_SCREEN_LAYOUT_SIZE_SMALL:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENSIZE) |
                                 ConfigDescription::SCREENSIZE_SMALL;
      break;

    case pb::Configuration_ScreenLayoutSize_SCREEN_LAYOUT_SIZE_NORMAL:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENSIZE) |
                                 ConfigDescription::SCREENSIZE_NORMAL;
      break;

    case pb::Configuration_ScreenLayoutSize_SCREEN_LAYOUT_SIZE_LARGE:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENSIZE) |
                                 ConfigDescription::SCREENSIZE_LARGE;
      break;

    case pb::Configuration_ScreenLayoutSize_SCREEN_LAYOUT_SIZE_XLARGE:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENSIZE) |
                                 ConfigDescription::SCREENSIZE_XLARGE;
      break;

    default:
      break;
  }

  switch (pb_config.screen_layout_long()) {
    case pb::Configuration_ScreenLayoutLong_SCREEN_LAYOUT_LONG_LONG:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENLONG) |
                                 ConfigDescription::SCREENLONG_YES;
      break;

    case pb::Configuration_ScreenLayoutLong_SCREEN_LAYOUT_LONG_NOTLONG:
      out_config->screenLayout = (out_config->screenLayout & ~ConfigDescription::MASK_SCREENLONG) |
                                 ConfigDescription::SCREENLONG_NO;
      break;

    default:
      break;
  }

  switch (pb_config.screen_round()) {
    case pb::Configuration_ScreenRound_SCREEN_ROUND_ROUND:
      out_config->screenLayout2 =
          (out_config->screenLayout2 & ~ConfigDescription::MASK_SCREENROUND) |
          ConfigDescription::SCREENROUND_YES;
      break;

    case pb::Configuration_ScreenRound_SCREEN_ROUND_NOTROUND:
      out_config->screenLayout2 =
          (out_config->screenLayout2 & ~ConfigDescription::MASK_SCREENROUND) |
          ConfigDescription::SCREENROUND_NO;
      break;

    default:
      break;
  }

  switch (pb_config.wide_color_gamut()) {
    case pb::Configuration_WideColorGamut_WIDE_COLOR_GAMUT_WIDECG:
      out_config->colorMode = (out_config->colorMode & ~ConfigDescription::MASK_WIDE_COLOR_GAMUT) |
                              ConfigDescription::WIDE_COLOR_GAMUT_YES;
      break;

    case pb::Configuration_WideColorGamut_WIDE_COLOR_GAMUT_NOWIDECG:
      out_config->colorMode = (out_config->colorMode & ~ConfigDescription::MASK_WIDE_COLOR_GAMUT) |
                              ConfigDescription::WIDE_COLOR_GAMUT_NO;
      break;

    default:
      break;
  }

  switch (pb_config.hdr()) {
    case pb::Configuration_Hdr_HDR_HIGHDR:
      out_config->colorMode =
          (out_config->colorMode & ~ConfigDescription::MASK_HDR) | ConfigDescription::HDR_YES;
      break;

    case pb::Configuration_Hdr_HDR_LOWDR:
      out_config->colorMode =
          (out_config->colorMode & ~ConfigDescription::MASK_HDR) | ConfigDescription::HDR_NO;
      break;

    default:
      break;
  }

  switch (pb_config.orientation()) {
    case pb::Configuration_Orientation_ORIENTATION_PORT:
      out_config->orientation = ConfigDescription::ORIENTATION_PORT;
      break;

    case pb::Configuration_Orientation_ORIENTATION_LAND:
      out_config->orientation = ConfigDescription::ORIENTATION_LAND;
      break;

    case pb::Configuration_Orientation_ORIENTATION_SQUARE:
      out_config->orientation = ConfigDescription::ORIENTATION_SQUARE;
      break;

    default:
      break;
  }

  switch (pb_config.ui_mode_type()) {
    case pb::Configuration_UiModeType_UI_MODE_TYPE_NORMAL:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_NORMAL;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_DESK:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_DESK;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_CAR:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_CAR;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_TELEVISION:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_TELEVISION;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_APPLIANCE:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_APPLIANCE;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_WATCH:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_WATCH;
      break;

    case pb::Configuration_UiModeType_UI_MODE_TYPE_VRHEADSET:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_TYPE) |
                           ConfigDescription::UI_MODE_TYPE_VR_HEADSET;
      break;

    default:
      break;
  }

  switch (pb_config.ui_mode_night()) {
    case pb::Configuration_UiModeNight_UI_MODE_NIGHT_NIGHT:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_NIGHT) |
                           ConfigDescription::UI_MODE_NIGHT_YES;
      break;

    case pb::Configuration_UiModeNight_UI_MODE_NIGHT_NOTNIGHT:
      out_config->uiMode = (out_config->uiMode & ~ConfigDescription::MASK_UI_MODE_NIGHT) |
                           ConfigDescription::UI_MODE_NIGHT_NO;
      break;

    default:
      break;
  }

  out_config->density = static_cast<uint16_t>(pb_config.density());

  switch (pb_config.touchscreen()) {
    case pb::Configuration_Touchscreen_TOUCHSCREEN_NOTOUCH:
      out_config->touchscreen = ConfigDescription::TOUCHSCREEN_NOTOUCH;
      break;

    case pb::Configuration_Touchscreen_TOUCHSCREEN_STYLUS:
      out_config->touchscreen = ConfigDescription::TOUCHSCREEN_STYLUS;
      break;

    case pb::Configuration_Touchscreen_TOUCHSCREEN_FINGER:
      out_config->touchscreen = ConfigDescription::TOUCHSCREEN_FINGER;
      break;

    default:
      break;
  }

  switch (pb_config.keys_hidden()) {
    case pb::Configuration_KeysHidden_KEYS_HIDDEN_KEYSEXPOSED:
      out_config->inputFlags = (out_config->inputFlags & ~ConfigDescription::MASK_KEYSHIDDEN) |
                               ConfigDescription::KEYSHIDDEN_NO;
      break;

    case pb::Configuration_KeysHidden_KEYS_HIDDEN_KEYSHIDDEN:
      out_config->inputFlags = (out_config->inputFlags & ~ConfigDescription::MASK_KEYSHIDDEN) |
                               ConfigDescription::KEYSHIDDEN_YES;
      break;

    case pb::Configuration_KeysHidden_KEYS_HIDDEN_KEYSSOFT:
      out_config->inputFlags = (out_config->inputFlags & ~ConfigDescription::MASK_KEYSHIDDEN) |
                               ConfigDescription::KEYSHIDDEN_SOFT;
      break;

    default:
      break;
  }

  switch (pb_config.keyboard()) {
    case pb::Configuration_Keyboard_KEYBOARD_NOKEYS:
      out_config->keyboard = ConfigDescription::KEYBOARD_NOKEYS;
      break;

    case pb::Configuration_Keyboard_KEYBOARD_QWERTY:
      out_config->keyboard = ConfigDescription::KEYBOARD_QWERTY;
      break;

    case pb::Configuration_Keyboard_KEYBOARD_TWELVEKEY:
      out_config->keyboard = ConfigDescription::KEYBOARD_12KEY;
      break;

    default:
      break;
  }

  switch (pb_config.nav_hidden()) {
    case pb::Configuration_NavHidden_NAV_HIDDEN_NAVEXPOSED:
      out_config->inputFlags = (out_config->inputFlags & ~ConfigDescription::MASK_NAVHIDDEN) |
                               ConfigDescription::NAVHIDDEN_NO;
      break;

    case pb::Configuration_NavHidden_NAV_HIDDEN_NAVHIDDEN:
      out_config->inputFlags = (out_config->inputFlags & ~ConfigDescription::MASK_NAVHIDDEN) |
                               ConfigDescription::NAVHIDDEN_YES;
      break;

    default:
      break;
  }

  switch (pb_config.navigation()) {
    case pb::Configuration_Navigation_NAVIGATION_NONAV:
      out_config->navigation = ConfigDescription::NAVIGATION_NONAV;
      break;

    case pb::Configuration_Navigation_NAVIGATION_DPAD:
      out_config->navigation = ConfigDescription::NAVIGATION_DPAD;
      break;

    case pb::Configuration_Navigation_NAVIGATION_TRACKBALL:
      out_config->navigation = ConfigDescription::NAVIGATION_TRACKBALL;
      break;

    case pb::Configuration_Navigation_NAVIGATION_WHEEL:
      out_config->navigation = ConfigDescription::NAVIGATION_WHEEL;
      break;

    default:
      break;
  }

  out_config->screenWidth = static_cast<uint16_t>(pb_config.screen_width());
  out_config->screenHeight = static_cast<uint16_t>(pb_config.screen_height());
  out_config->sdkVersion = static_cast<uint16_t>(pb_config.sdk_version());
  return true;
}

static void DeserializeSourceFromPb(const pb::Source& pb_source, const ResStringPool& src_pool,
                                    Source* out_source) {
  out_source->path = util::GetString(src_pool, pb_source.path_idx());
  out_source->line = static_cast<size_t>(pb_source.position().line_number());
}

static SymbolState DeserializeVisibilityFromPb(const pb::SymbolStatus_Visibility& pb_visibility) {
  switch (pb_visibility) {
    case pb::SymbolStatus_Visibility_PRIVATE:
      return SymbolState::kPrivate;
    case pb::SymbolStatus_Visibility_PUBLIC:
      return SymbolState::kPublic;
    default:
      break;
  }
  return SymbolState::kUndefined;
}

static bool DeserializePackageFromPb(const pb::Package& pb_package, const ResStringPool& src_pool,
                                     io::IFileCollection* files, ResourceTable* out_table,
                                     std::string* out_error) {
  Maybe<uint8_t> id;
  if (pb_package.has_package_id()) {
    id = static_cast<uint8_t>(pb_package.package_id().id());
  }

  std::map<ResourceId, ResourceNameRef> id_index;

  ResourceTablePackage* pkg = out_table->CreatePackage(pb_package.package_name(), id);
  for (const pb::Type& pb_type : pb_package.type()) {
    const ResourceType* res_type = ParseResourceType(pb_type.name());
    if (res_type == nullptr) {
      std::ostringstream error;
      error << "unknown type '" << pb_type.name() << "'";
      *out_error = error.str();
      return false;
    }

    ResourceTableType* type = pkg->FindOrCreateType(*res_type);
    if (pb_type.has_type_id()) {
      type->id = static_cast<uint8_t>(pb_type.type_id().id());
    }

    for (const pb::Entry& pb_entry : pb_type.entry()) {
      ResourceEntry* entry = type->FindOrCreateEntry(pb_entry.name());
      if (pb_entry.has_entry_id()) {
        entry->id = static_cast<uint16_t>(pb_entry.entry_id().id());
      }

      // Deserialize the symbol status (public/private with source and comments).
      if (pb_entry.has_symbol_status()) {
        const pb::SymbolStatus& pb_status = pb_entry.symbol_status();
        if (pb_status.has_source()) {
          DeserializeSourceFromPb(pb_status.source(), src_pool, &entry->symbol_status.source);
        }

        entry->symbol_status.comment = pb_status.comment();
        entry->symbol_status.allow_new = pb_status.allow_new();

        const SymbolState visibility = DeserializeVisibilityFromPb(pb_status.visibility());
        entry->symbol_status.state = visibility;
        if (visibility == SymbolState::kPublic) {
          // Propagate the public visibility up to the Type.
          type->symbol_status.state = SymbolState::kPublic;
        } else if (visibility == SymbolState::kPrivate) {
          // Only propagate if no previous state was assigned.
          if (type->symbol_status.state == SymbolState::kUndefined) {
            type->symbol_status.state = SymbolState::kPrivate;
          }
        }
      }

      ResourceId resid(pb_package.package_id().id(), pb_type.type_id().id(),
                       pb_entry.entry_id().id());
      if (resid.is_valid()) {
        id_index[resid] = ResourceNameRef(pkg->name, type->type, entry->name);
      }

      for (const pb::ConfigValue& pb_config_value : pb_entry.config_value()) {
        const pb::Configuration& pb_config = pb_config_value.config();

        ConfigDescription config;
        if (!DeserializeConfigFromPb(pb_config, &config, out_error)) {
          return false;
        }

        ResourceConfigValue* config_value = entry->FindOrCreateValue(config, pb_config.product());
        if (config_value->value != nullptr) {
          *out_error = "duplicate configuration in resource table";
          return false;
        }

        config_value->value = DeserializeValueFromPb(pb_config_value.value(), src_pool, config,
                                                     &out_table->string_pool, files, out_error);
        if (config_value->value == nullptr) {
          return false;
        }
      }
    }
  }

  ReferenceIdToNameVisitor visitor(&id_index);
  VisitAllValuesInPackage(pkg, &visitor);
  return true;
}

bool DeserializeTableFromPb(const pb::ResourceTable& pb_table, io::IFileCollection* files,
                            ResourceTable* out_table, std::string* out_error) {
  // We import the android namespace because on Windows NO_ERROR is a macro, not an enum, which
  // causes errors when qualifying it with android::
  using namespace android;

  ResStringPool source_pool;
  if (pb_table.has_source_pool()) {
    status_t result = source_pool.setTo(pb_table.source_pool().data().data(),
                                        pb_table.source_pool().data().size());
    if (result != NO_ERROR) {
      *out_error = "invalid source pool";
      return false;
    }
  }

  for (const pb::Package& pb_package : pb_table.package()) {
    if (!DeserializePackageFromPb(pb_package, source_pool, files, out_table, out_error)) {
      return false;
    }
  }
  return true;
}

static ResourceFile::Type DeserializeFileReferenceTypeFromPb(const pb::FileReference::Type& type) {
  switch (type) {
    case pb::FileReference::BINARY_XML:
      return ResourceFile::Type::kBinaryXml;
    case pb::FileReference::PROTO_XML:
      return ResourceFile::Type::kProtoXml;
    case pb::FileReference::PNG:
      return ResourceFile::Type::kPng;
    default:
      return ResourceFile::Type::kUnknown;
  }
}

bool DeserializeCompiledFileFromPb(const pb::internal::CompiledFile& pb_file,
                                   ResourceFile* out_file, std::string* out_error) {
  ResourceNameRef name_ref;
  if (!ResourceUtils::ParseResourceName(pb_file.resource_name(), &name_ref)) {
    std::ostringstream error;
    error << "invalid resource name in compiled file header: " << pb_file.resource_name();
    *out_error = error.str();
    return false;
  }

  out_file->name = name_ref.ToResourceName();
  out_file->source.path = pb_file.source_path();
  out_file->type = DeserializeFileReferenceTypeFromPb(pb_file.type());

  std::string config_error;
  if (!DeserializeConfigFromPb(pb_file.config(), &out_file->config, &config_error)) {
    std::ostringstream error;
    error << "invalid resource configuration in compiled file header: " << config_error;
    *out_error = error.str();
    return false;
  }

  for (const pb::internal::CompiledFile_Symbol& pb_symbol : pb_file.exported_symbol()) {
    if (!ResourceUtils::ParseResourceName(pb_symbol.resource_name(), &name_ref)) {
      std::ostringstream error;
      error << "invalid resource name for exported symbol in compiled file header: "
            << pb_file.resource_name();
      *out_error = error.str();
      return false;
    }

    size_t line = 0u;
    if (pb_symbol.has_source()) {
      line = pb_symbol.source().line_number();
    }
    out_file->exported_symbols.push_back(SourcedResourceName{name_ref.ToResourceName(), line});
  }
  return true;
}

static Reference::Type DeserializeReferenceTypeFromPb(const pb::Reference_Type& pb_type) {
  switch (pb_type) {
    case pb::Reference_Type_REFERENCE:
      return Reference::Type::kResource;
    case pb::Reference_Type_ATTRIBUTE:
      return Reference::Type::kAttribute;
    default:
      break;
  }
  return Reference::Type::kResource;
}

static bool DeserializeReferenceFromPb(const pb::Reference& pb_ref, Reference* out_ref,
                                       std::string* out_error) {
  out_ref->reference_type = DeserializeReferenceTypeFromPb(pb_ref.type());
  out_ref->private_reference = pb_ref.private_();

  if (pb_ref.id() != 0) {
    out_ref->id = ResourceId(pb_ref.id());
  }

  if (!pb_ref.name().empty()) {
    ResourceNameRef name_ref;
    if (!ResourceUtils::ParseResourceName(pb_ref.name(), &name_ref, nullptr)) {
      std::ostringstream error;
      error << "reference has invalid resource name '" << pb_ref.name() << "'";
      *out_error = error.str();
      return false;
    }
    out_ref->name = name_ref.ToResourceName();
  }
  return true;
}

template <typename T>
static void DeserializeItemMetaDataFromPb(const T& pb_item, const android::ResStringPool& src_pool,
                                          Value* out_value) {
  if (pb_item.has_source()) {
    Source source;
    DeserializeSourceFromPb(pb_item.source(), src_pool, &source);
    out_value->SetSource(std::move(source));
  }
  out_value->SetComment(pb_item.comment());
}

static size_t DeserializePluralEnumFromPb(const pb::Plural_Arity& arity) {
  switch (arity) {
    case pb::Plural_Arity_ZERO:
      return Plural::Zero;
    case pb::Plural_Arity_ONE:
      return Plural::One;
    case pb::Plural_Arity_TWO:
      return Plural::Two;
    case pb::Plural_Arity_FEW:
      return Plural::Few;
    case pb::Plural_Arity_MANY:
      return Plural::Many;
    default:
      break;
  }
  return Plural::Other;
}

std::unique_ptr<Value> DeserializeValueFromPb(const pb::Value& pb_value,
                                              const android::ResStringPool& src_pool,
                                              const ConfigDescription& config,
                                              StringPool* value_pool, io::IFileCollection* files,
                                              std::string* out_error) {
  std::unique_ptr<Value> value;
  if (pb_value.has_item()) {
    value = DeserializeItemFromPb(pb_value.item(), src_pool, config, value_pool, files, out_error);
    if (value == nullptr) {
      return {};
    }

  } else if (pb_value.has_compound_value()) {
    const pb::CompoundValue& pb_compound_value = pb_value.compound_value();
    switch (pb_compound_value.value_case()) {
      case pb::CompoundValue::kAttr: {
        const pb::Attribute& pb_attr = pb_compound_value.attr();
        std::unique_ptr<Attribute> attr = util::make_unique<Attribute>();
        attr->type_mask = pb_attr.format_flags();
        attr->min_int = pb_attr.min_int();
        attr->max_int = pb_attr.max_int();
        for (const pb::Attribute_Symbol& pb_symbol : pb_attr.symbol()) {
          Attribute::Symbol symbol;
          DeserializeItemMetaDataFromPb(pb_symbol, src_pool, &symbol.symbol);
          if (!DeserializeReferenceFromPb(pb_symbol.name(), &symbol.symbol, out_error)) {
            return {};
          }
          symbol.value = pb_symbol.value();
          attr->symbols.push_back(std::move(symbol));
        }
        value = std::move(attr);
      } break;

      case pb::CompoundValue::kStyle: {
        const pb::Style& pb_style = pb_compound_value.style();
        std::unique_ptr<Style> style = util::make_unique<Style>();
        if (pb_style.has_parent()) {
          style->parent = Reference();
          if (!DeserializeReferenceFromPb(pb_style.parent(), &style->parent.value(), out_error)) {
            return {};
          }

          if (pb_style.has_parent_source()) {
            Source parent_source;
            DeserializeSourceFromPb(pb_style.parent_source(), src_pool, &parent_source);
            style->parent.value().SetSource(std::move(parent_source));
          }
        }

        for (const pb::Style_Entry& pb_entry : pb_style.entry()) {
          Style::Entry entry;
          if (!DeserializeReferenceFromPb(pb_entry.key(), &entry.key, out_error)) {
            return {};
          }
          DeserializeItemMetaDataFromPb(pb_entry, src_pool, &entry.key);
          entry.value = DeserializeItemFromPb(pb_entry.item(), src_pool, config, value_pool, files,
                                              out_error);
          if (entry.value == nullptr) {
            return {};
          }

          // Copy the meta-data into the value as well.
          DeserializeItemMetaDataFromPb(pb_entry, src_pool, entry.value.get());
          style->entries.push_back(std::move(entry));
        }
        value = std::move(style);
      } break;

      case pb::CompoundValue::kStyleable: {
        const pb::Styleable& pb_styleable = pb_compound_value.styleable();
        std::unique_ptr<Styleable> styleable = util::make_unique<Styleable>();
        for (const pb::Styleable_Entry& pb_entry : pb_styleable.entry()) {
          Reference attr_ref;
          DeserializeItemMetaDataFromPb(pb_entry, src_pool, &attr_ref);
          DeserializeReferenceFromPb(pb_entry.attr(), &attr_ref, out_error);
          styleable->entries.push_back(std::move(attr_ref));
        }
        value = std::move(styleable);
      } break;

      case pb::CompoundValue::kArray: {
        const pb::Array& pb_array = pb_compound_value.array();
        std::unique_ptr<Array> array = util::make_unique<Array>();
        for (const pb::Array_Element& pb_entry : pb_array.element()) {
          std::unique_ptr<Item> item = DeserializeItemFromPb(pb_entry.item(), src_pool, config,
                                                             value_pool, files, out_error);
          if (item == nullptr) {
            return {};
          }

          DeserializeItemMetaDataFromPb(pb_entry, src_pool, item.get());
          array->elements.push_back(std::move(item));
        }
        value = std::move(array);
      } break;

      case pb::CompoundValue::kPlural: {
        const pb::Plural& pb_plural = pb_compound_value.plural();
        std::unique_ptr<Plural> plural = util::make_unique<Plural>();
        for (const pb::Plural_Entry& pb_entry : pb_plural.entry()) {
          size_t plural_idx = DeserializePluralEnumFromPb(pb_entry.arity());
          plural->values[plural_idx] = DeserializeItemFromPb(pb_entry.item(), src_pool, config,
                                                             value_pool, files, out_error);
          if (!plural->values[plural_idx]) {
            return {};
          }

          DeserializeItemMetaDataFromPb(pb_entry, src_pool, plural->values[plural_idx].get());
        }
        value = std::move(plural);
      } break;

      default:
        LOG(FATAL) << "unknown compound value: " << (int)pb_compound_value.value_case();
        break;
    }
  } else {
    LOG(FATAL) << "unknown value: " << (int)pb_value.value_case();
    return {};
  }

  CHECK(value) << "forgot to set value";

  value->SetWeak(pb_value.weak());
  DeserializeItemMetaDataFromPb(pb_value, src_pool, value.get());
  return value;
}

std::unique_ptr<Item> DeserializeItemFromPb(const pb::Item& pb_item,
                                            const android::ResStringPool& src_pool,
                                            const ConfigDescription& config, StringPool* value_pool,
                                            io::IFileCollection* files, std::string* out_error) {
  switch (pb_item.value_case()) {
    case pb::Item::kRef: {
      const pb::Reference& pb_ref = pb_item.ref();
      std::unique_ptr<Reference> ref = util::make_unique<Reference>();
      if (!DeserializeReferenceFromPb(pb_ref, ref.get(), out_error)) {
        return {};
      }
      return std::move(ref);
    } break;

    case pb::Item::kPrim: {
      const pb::Primitive& pb_prim = pb_item.prim();
      return util::make_unique<BinaryPrimitive>(static_cast<uint8_t>(pb_prim.type()),
                                                pb_prim.data());
    } break;

    case pb::Item::kId: {
      return util::make_unique<Id>();
    } break;

    case pb::Item::kStr: {
      return util::make_unique<String>(
          value_pool->MakeRef(pb_item.str().value(), StringPool::Context(config)));
    } break;

    case pb::Item::kRawStr: {
      return util::make_unique<RawString>(
          value_pool->MakeRef(pb_item.raw_str().value(), StringPool::Context(config)));
    } break;

    case pb::Item::kStyledStr: {
      const pb::StyledString& pb_str = pb_item.styled_str();
      StyleString style_str{pb_str.value()};
      for (const pb::StyledString::Span& pb_span : pb_str.span()) {
        style_str.spans.push_back(Span{pb_span.tag(), pb_span.first_char(), pb_span.last_char()});
      }
      return util::make_unique<StyledString>(value_pool->MakeRef(
          style_str, StringPool::Context(StringPool::Context::kNormalPriority, config)));
    } break;

    case pb::Item::kFile: {
      const pb::FileReference& pb_file = pb_item.file();
      std::unique_ptr<FileReference> file_ref =
          util::make_unique<FileReference>(value_pool->MakeRef(
              pb_file.path(), StringPool::Context(StringPool::Context::kHighPriority, config)));
      file_ref->type = DeserializeFileReferenceTypeFromPb(pb_file.type());
      if (files != nullptr) {
        file_ref->file = files->FindFile(*file_ref->path);
      }
      return std::move(file_ref);
    } break;

    default:
      LOG(FATAL) << "unknown item: " << (int)pb_item.value_case();
      break;
  }
  return {};
}

std::unique_ptr<xml::XmlResource> DeserializeXmlResourceFromPb(const pb::XmlNode& pb_node,
                                                               std::string* out_error) {
  if (!pb_node.has_element()) {
    return {};
  }

  std::unique_ptr<xml::XmlResource> resource = util::make_unique<xml::XmlResource>();
  resource->root = util::make_unique<xml::Element>();
  if (!DeserializeXmlFromPb(pb_node, resource->root.get(), &resource->string_pool, out_error)) {
    return {};
  }
  return resource;
}

bool DeserializeXmlFromPb(const pb::XmlNode& pb_node, xml::Element* out_el, StringPool* value_pool,
                          std::string* out_error) {
  const pb::XmlElement& pb_el = pb_node.element();
  out_el->name = pb_el.name();
  out_el->namespace_uri = pb_el.namespace_uri();
  out_el->line_number = pb_node.source().line_number();
  out_el->column_number = pb_node.source().column_number();

  for (const pb::XmlNamespace& pb_ns : pb_el.namespace_declaration()) {
    xml::NamespaceDecl decl;
    decl.uri = pb_ns.uri();
    decl.prefix = pb_ns.prefix();
    decl.line_number = pb_ns.source().line_number();
    decl.column_number = pb_ns.source().column_number();
    out_el->namespace_decls.push_back(std::move(decl));
  }

  for (const pb::XmlAttribute& pb_attr : pb_el.attribute()) {
    xml::Attribute attr;
    attr.name = pb_attr.name();
    attr.namespace_uri = pb_attr.namespace_uri();
    attr.value = pb_attr.value();
    if (pb_attr.resource_id() != 0u) {
      attr.compiled_attribute = xml::AaptAttribute{Attribute(), ResourceId(pb_attr.resource_id())};
    }
    if (pb_attr.has_compiled_item()) {
      attr.compiled_value =
          DeserializeItemFromPb(pb_attr.compiled_item(), {}, {}, value_pool, nullptr, out_error);
      if (attr.compiled_value == nullptr) {
        return {};
      }
      attr.compiled_value->SetSource(Source().WithLine(pb_attr.source().line_number()));
    }
    out_el->attributes.push_back(std::move(attr));
  }

  // Deserialize the children.
  for (const pb::XmlNode& pb_child : pb_el.child()) {
    switch (pb_child.node_case()) {
      case pb::XmlNode::NodeCase::kText: {
        std::unique_ptr<xml::Text> text = util::make_unique<xml::Text>();
        text->line_number = pb_child.source().line_number();
        text->column_number = pb_child.source().column_number();
        text->text = pb_child.text();
        out_el->AppendChild(std::move(text));
      } break;

      case pb::XmlNode::NodeCase::kElement: {
        std::unique_ptr<xml::Element> child_el = util::make_unique<xml::Element>();
        if (!DeserializeXmlFromPb(pb_child, child_el.get(), value_pool, out_error)) {
          return false;
        }
        out_el->AppendChild(std::move(child_el));
      } break;

      default:
        LOG(FATAL) << "unknown XmlNode " << (int)pb_child.node_case();
        break;
    }
  }
  return true;
}

}  // namespace aapt
