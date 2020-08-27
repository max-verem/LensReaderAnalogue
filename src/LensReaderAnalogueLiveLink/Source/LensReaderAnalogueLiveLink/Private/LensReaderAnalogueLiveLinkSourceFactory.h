#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceFactory.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "LensReaderAnalogueLiveLinkSourceFactory.generated.h"

/**
 * 
 */
UCLASS()
class LENSREADERANALOGUELIVELINK_API ULensReaderAnalogueLiveLinkSourceFactory : public ULiveLinkSourceFactory
{
public:
    GENERATED_BODY()

    virtual FText GetSourceDisplayName() const override;
    virtual FText GetSourceTooltip() const override;

    virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
    virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
    TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;
private:
    void OnOkClicked(FText Endpoint, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};
